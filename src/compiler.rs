use cranelift_codegen::ir::condcodes::IntCC;
use cranelift_codegen::{ir::AbiParam, settings};
use cranelift_codegen::ir::{self, Block, InstBuilder, MemFlagsData, StackSlotData, StackSlotKind, types};
use cranelift_frontend::{FunctionBuilder, FunctionBuilderContext, Variable};
use cranelift_module::{DataDescription, Linkage, Module, ModuleRelocTarget, default_libcall_names};
use cranelift_native::builder;
use cranelift_object::{ObjectBuilder, ObjectModule};
use miette::NamedSource;
use std::cell::RefCell;

use crate::ast::ASTType;
use crate::comet_error::{CompilerBug, ImmutableReassignment, InvalidCompilerDirective, InvalidLValue, InvalidOperator, NotAFunction, SyntaxError, TypeMismatch, UndefinedVariable, UnkownField, UnkownType};
use crate::comet_struct::{CometStruct, CometStructField};
use crate::comet_type::{CometFunction, CometTypeKind};
use crate::scope::CometVarType::{External, Local};
use crate::scope::{CometVarType, CometVariable, ScopeFrame};
use crate::{ast::{ASTNode, ASTNodeType}, comet_type::CometType, token::TokenType};

pub struct Compiler <'a> {
    module: ObjectModule,
    scopes: Vec<ScopeFrame<'a>>,
    file_name: &'a str,
    source: String,

    var_index: u32
}

impl <'a> Compiler <'a> {
    pub fn new(file_name: &'a str, source: String) -> miette::Result<Self, Box<dyn std::error::Error>> {
        let isa_builder = cranelift_native::builder()?;
        let isa = isa_builder.finish(settings::Flags::new(settings::builder()))?;

        let obj_builder = ObjectBuilder::new(
            isa,
            "comet_module",
            default_libcall_names()
        )?;

        let module = ObjectModule::new(obj_builder);

        let mut base_frame = ScopeFrame::new();

        base_frame.types.insert("bool", CometType::new(types::I8));
        base_frame.types.insert("i8", CometType::new_int(types::I8, true));
        base_frame.types.insert("i16", CometType::new_int(types::I16, true));
        base_frame.types.insert("i32", CometType::new_int(types::I32, true));
        base_frame.types.insert("i64", CometType::new_int(types::I64, true));
        base_frame.types.insert("u8", CometType::new_int(types::I8, false));
        base_frame.types.insert("u16", CometType::new_int(types::I16, false));
        base_frame.types.insert("u32", CometType::new_int(types::I32, false));
        base_frame.types.insert("u64", CometType::new_int(types::I64, false));
        base_frame.types.insert("f16", CometType::new(types::F16));
        base_frame.types.insert("f32", CometType::new(types::F32));
        base_frame.types.insert("f64", CometType::new(types::F64));
        base_frame.types.insert("ptr", CometType::new_ptr(module.isa().pointer_type()));
        

        Ok(Compiler {
            module: module,
            scopes: vec!{base_frame},
            file_name: file_name,
            source: source,

            var_index: 0
        })
    }

    pub fn end_module(self) -> miette::Result<Vec<u8>, cranelift_object::object::write::Error> {
        let module = self.module;

        let product = module.finish();
        let bytes = product.emit();

        return bytes;
    }

    // UTIL METHODS //
    fn named_source(&self) -> NamedSource<String> {
        NamedSource::new(self.file_name, self.source.clone())
    }

    fn get_variable(&self, name: &str) -> Option<&CometVariable> {
        self.scopes.iter().rev().find_map(|scope| scope.variables.get(name))
    }

    fn get_type(&self, name: &str) -> Option<&CometType> {
        self.scopes.iter().rev().find_map(|scope| scope.types.get(name))
    }

    fn get_type_literal_type (&self, node: &ASTNode) -> miette::Result<CometType> {

        let ast_type = match node.node_type() {
            ASTNodeType::TypeLiteral(value) => value,
            _ => unreachable!()
        };

        match ast_type {
            ASTType::Identifier(struct_name) => {
                let ident_node = match struct_name.as_ref().node_type() {
                    ASTNodeType::IdentifierLiteral(value) => value,
                    _ => unreachable!()
                };

                self.get_type(ident_node.as_str()).cloned().ok_or(UnkownType {
                    span: struct_name.source_span(),
                    src: self.named_source(),
                    type_: ident_node.clone()
                }.into())
            },

            ASTType::Function { arg_types, return_type: return_type_node } => {
                let compiled_arg_types: miette::Result<Vec<CometType>> = arg_types
                                            .iter()
                                            .map(|t| self.get_type_literal_type(t))
                                            .collect();
                let compiled_arg_types = compiled_arg_types?;

                let return_type = return_type_node
                    .as_deref()
                    .map(|t| self.get_type_literal_type(t))
                    .transpose()?
                    .unwrap_or_else(|| CometType::new(types::INVALID));

                Ok(CometType::new_function(CometFunction {
                    arg_types: compiled_arg_types,
                    return_type: Box::new(return_type)
                }))
            }
        }
        
    }

    fn rank_type(&self, type_: &CometType) -> u8 {
        match type_.cranelift_type {
            types::I8 => 0,
            types::I16 => 1,
            types::I32 => 2,
            types::I64 => 3,
            types::F16 => 4,
            types::F32 => 5,
            types::F64 => 6,
            _ => 0
        }
    }

    fn unify_types(&self, a: &'a CometType, b: &'a CometType) -> &CometType {
        if self.rank_type(a) > self.rank_type(b) {
            a
        } else {
            b
        }
    }

    fn ensure_share_types(&mut self, left: &ASTNode, right: &ASTNode, builder: &mut FunctionBuilder) -> miette::Result<(ir::Value, ir::Value)> {
        let mut left_value = self.visit_value(left, builder)?;
        let mut right_value = self.visit_value(right, builder)?;

        let left_type = self.resolve_type(left)?;
        let right_type = self.resolve_type(right)?;

        let unified_type = self.unify_types(&left_type, &right_type);

        if left_type != right_type {
            left_value = CometType::try_implicit_cast(left, left_value, &left_type, unified_type, builder, self.named_source())?;
            right_value = CometType::try_implicit_cast(right, right_value, &right_type, unified_type, builder, self.named_source())?;
        }

        Ok((left_value, right_value))
    }

    fn resolve_type(&self, node: &ASTNode) -> miette::Result<CometType> {
        match node.node_type() {
            ASTNodeType::IntLiteral(_) => {
                Ok(CometType::new_int(types::I64, false))
            },

            ASTNodeType::FloatLiteral(_) => {
                Ok(CometType::new(types::F64))
            },

            ASTNodeType::StringLiteral(_) => {
                Ok(CometType::new_ptr(self.module.isa().pointer_type()))
            },

            ASTNodeType::IdentifierLiteral(name) => {
                let var = self.get_variable(&name.as_str()).ok_or(format!("Use of undefined variable '{}'", name)).unwrap();

                Ok(var.type_.clone())
            },

            ASTNodeType::InfixExpression { left, op, right } => {
                let left_value = self.resolve_type(left)?;

                match op.token_type() {
                    TokenType::Dot => {
                        let struct_type = match left_value.kind {
                            CometTypeKind::Struct(comet_struct) => comet_struct,
                            _ => { return Err(InvalidOperator {
                                    op: op.token_type().clone(),
                                    span: op.source_span(),
                                    src: self.named_source(),
                                    value: String::from("on non-struct")
                                }.into()); }
                        };

                        let field_name = match right.node_type() {
                            ASTNodeType::IdentifierLiteral(value) => value,
                            _ => return Err(SyntaxError {
                                src: self.named_source(),
                                text: format!("Expected identifier after '{:?}', got '{:?}' instead", op.token_type(), right.node_type()),
                                span: right.source_span()
                            }.into())
                        };

                        let field = struct_type.get_field(field_name).ok_or(UnkownField {
                            field: field_name.clone(),
                            struct_name: String::from(struct_type.name()),
                            span: right.source_span(),
                            src: self.named_source()
                        })?;

                        return Ok(field.field_type().clone());
                    },
                    TokenType::Divide => { return Ok(CometType::new(types::F64)); },

                    TokenType::Eq | TokenType::NotEq |
                    TokenType::Gt | TokenType::Lt |
                    TokenType::GtEq | TokenType::LtEq => { return Ok(CometType::new_int(types::I8, false)); },


                    _ => {}
                }

                let right_value = self.resolve_type(right)?;
                return Ok(self.unify_types(&left_value, &right_value).clone());
            },

            ASTNodeType::FuncCall { left: _, args: _ } => {
                /*let decls = self.module.declarations();

                let func_id = self.get_function(left)?;
                let func_decl = decls.get_function_decl(func_id);
                let returns = &func_decl.signature.returns;

                let out_type: CometType;
                if returns.len() == 0 {
                    out_type = CometType { cranelift_type: types::INVALID }
                } else {
                    out_type = CometType { cranelift_type: returns[0].value_type }
                }*/

                // TODO:
                let out_type = CometType::new_int(types::I64, true);

                Ok(out_type)
            },

            ASTNodeType::StructCreateExpression { type_, fields: _ } => {
                let struct_type = self.get_type_literal_type(type_)?;
                Ok(struct_type.clone())
            }

            _ => Err(CompilerBug {
                src: self.named_source(),
                span: node.source_span(),
                text: format!("Can't resolve type of '{:?}'", node.node_type())
            }.into())
        }
    }
    // END OF UTIL METHODS

    // VISIT METHODS //
    fn visit_program(&mut self, node: &'a ASTNode) -> miette::Result<()> {
        let nodes = match node.node_type() {
            ASTNodeType::Program(value) => value,
            _ => unreachable!()
        };

        for node in nodes {
            self.compile(node, None)?;
        }

        Ok(())
    }

    fn visit_block(&mut self, node: &'a ASTNode, builder: &mut FunctionBuilder) -> miette::Result<()> {
        let nodes = match node.node_type() {
            ASTNodeType::Block(value) => value,
            _ => unreachable!()
        };

        for node in nodes {
            self.compile(node, Some(builder))?;
        }

        Ok(())
    }

    fn visit_value(&mut self, node: &ASTNode, builder: &mut FunctionBuilder) -> miette::Result<ir::Value> {
        match node.node_type() {
            ASTNodeType::InfixExpression { left: _, op, right: _ } => {
                if op.token_type() == &TokenType::Dot {
                    self.visit_get_field(node, builder)
                } else {
                    self.visit_infix_expression(node, builder)
                }
            },

            ASTNodeType::IntLiteral(num) => {
                Ok(builder.ins().iconst(types::I64, *num as i64))
            },

            ASTNodeType::FloatLiteral(num) => {
                Ok(builder.ins().f64const(*num as f64))
            },

            ASTNodeType::StringLiteral(str) => {
                let mut data_desc = DataDescription::new();
                data_desc.define(str.as_bytes().to_vec().into_boxed_slice());

                let data_id = self.module
                    .declare_data("string_literal", Linkage::Local, false, false).unwrap();

                self.module.define_data(data_id, &data_desc).unwrap();

                let local_data_id = self.module.declare_data_in_func(data_id, &mut builder.func);

                let pointer_type = self.module.isa().pointer_type();

                let ptr = builder.ins().symbol_value(pointer_type, local_data_id);

                Ok(ptr)
            },

            ASTNodeType::IdentifierLiteral(var_name) => {
                let comet_var: &CometVariable = self.get_variable(&var_name.as_str()).ok_or(UndefinedVariable {
                    span: node.source_span(),
                    src: self.named_source(),
                    var: var_name.clone()
                })?;

                if let Some(func_id) = comet_var.function_id {
                    let func_ref = self.module.declare_func_in_func(func_id, builder.func);
                    let pointer_type = self.module.isa().frontend_config().pointer_type();
                    return Ok(builder.ins().func_addr(pointer_type, func_ref));
                }

                match comet_var.var_type {
                    CometVarType::Local(var) => Ok(builder.use_var(var)),
                    CometVarType::FuncArg(index) => {
                        let params = builder.block_params(builder.current_block().unwrap());
                        Ok(params[index])
                    },
                    CometVarType::External(data) => {
                        let global_value = self.module.declare_data_in_func(data, &mut builder.func);
                        let ptr_type = self.module.target_config().pointer_type();

                        let data_ptr = builder.ins().symbol_value(ptr_type, global_value);
                        Ok(builder.ins().load(
                            ptr_type,
                            MemFlagsData::new(),
                            data_ptr,
                            0
                        ))
                    }
                }  
            },

            ASTNodeType::FuncCall { left, args } => {
                let func_ptr = self.visit_value(left, builder)?;

                let mut compiled_args: Vec<ir::Value> = vec![];
                for arg in args {
                    compiled_args.push(self.visit_value(arg, builder)?);
                }

                let func_id = match left.node_type() {
                    ASTNodeType::IdentifierLiteral(name) => self.get_variable(name)
                        .and_then(|variable| variable.function_id)
                        .ok_or(NotAFunction {
                            span: (**left).source_span(),
                            src: NamedSource::new(String::from(self.file_name), self.source.clone()),
                            func_name: name.clone(),
                        })?,
                    _ => return Err(SyntaxError {
                            span: (**left).source_span(),
                            src: NamedSource::new(String::from(self.file_name), self.source.clone()),
                            text: String::from("Expected identifier")
                        }.into())
                };

                let decls = self.module.declarations();
                let func_decl_cell = RefCell::new(decls.get_function_decl(func_id));
                let func_decl = func_decl_cell.borrow();

                let sig = func_decl.signature.clone();
                let sig_ref = builder.import_signature(sig);

                let call_inst = builder.ins().call_indirect(sig_ref, func_ptr, &compiled_args);
                let results = builder.inst_results(call_inst);

                Ok(results[0])
                
            },

            ASTNodeType::StructCreateExpression { type_: type_node, fields } => {
                let struct_type = self.get_type_literal_type(type_node)?;

                let comet_struct = match struct_type.kind {
                    CometTypeKind::Struct(value) => value,

                    _ => { return Err(TypeMismatch {
                        src: self.named_source(),
                        expected: String::from("struct"),
                        invalid: struct_type.cranelift_type.to_string(),
                        span: type_node.source_span()
                    }.into()); }
                };

                let struct_layout = comet_struct.get_layout();

                let stack_slot = builder.create_sized_stack_slot(StackSlotData::new( 
                    StackSlotKind::ExplicitSlot,
                    struct_layout.1,
                    8
                ));
                let addr = builder.ins().stack_addr(types::I64, stack_slot, 0);

                let fields_layout = struct_layout.0;

                for field in fields {
                    let (ident_node, value_node) = match field.node_type() {
                        ASTNodeType::StructField { ident, value } => (ident, value),
                        _ => unreachable!()
                    };

                    let field_name = match ident_node.node_type() {
                        ASTNodeType::IdentifierLiteral(value) => value,
                        _ => unreachable!()
                    };

                    let field_value = self.visit_value(&value_node, builder)?;

                    let field_index = comet_struct.get_field_index(&field_name);
                    if field_index.is_none() {
                        return Err(UnkownField {
                            field: field_name.clone(),
                            struct_name: String::from(comet_struct.name()),
                            span: ident_node.source_span(),
                            src: self.named_source()
                        }.into());
                    }
                    let field_index = field_index.unwrap();

                    let field_offset = fields_layout[*field_index];
                    builder.ins().store(MemFlagsData::new(), field_value, addr, field_offset as i32);
                }
                

                Ok(addr)
            },

            _ => Err(CompilerBug {
                src: self.named_source(),
                span: node.source_span(),
                text: format!("Cannot compile r-value '{:?}'", node.node_type())
            }.into())
        }
    }

    fn visit_l_value(&mut self, node: &ASTNode, builder: &mut FunctionBuilder) -> miette::Result<ir::Value> {
        match node.node_type() {
            ASTNodeType::IdentifierLiteral(_) => self.visit_value(node, builder),

            ASTNodeType::InfixExpression { left, op, right } => {
                let left_type = self.resolve_type(left)?;
                let left = self.visit_l_value(left, builder)?;

                match op.token_type() {
                    TokenType::Dot => {
                        let comet_struct = match left_type.kind {
                            CometTypeKind::Struct(value) => value,
                            _ => {  return Err(InvalidOperator {
                                        op: op.token_type().clone(),
                                        span: op.source_span(),
                                        src: self.named_source(),
                                        value: String::from("on non-struct")
                                    }.into()); }
                        };

                        let field_name = match right.node_type() {
                            ASTNodeType::IdentifierLiteral(value) => value,
                            _ => { return Err(SyntaxError {
                                src: self.named_source(),
                                text: format!("Expected identifier after '{:?}', got '{:?}' instead", op.token_type(), right.node_type()),
                                span: right.source_span()
                            }.into()); }
                        };

                        let comet_struct_layout = comet_struct.get_layout();

                        let field = comet_struct.get_field(field_name);
                        if field.is_none() {
                            return Err(UnkownField {
                                field: field_name.clone(),
                                struct_name: String::from(comet_struct.name()),
                                span: right.source_span(),
                                src: self.named_source()
                            }.into());
                        }
                        let field_index = comet_struct.get_field_index(field_name).unwrap();

                        Ok(builder.ins().iadd_imm_s(left, comet_struct_layout.0[*field_index] as i64))
                    },


                    _ => Err(InvalidOperator {
                            op: op.token_type().clone(),
                            span: op.source_span(),
                            src: self.named_source(),
                            value: String::from("on l-value")
                        }.into())
                }
            },
            _ => Err(InvalidLValue {
                src: self.named_source(),
                span: node.source_span(),
                value: format!("{:?}", node.node_type())
            }.into())
        }
    }

    fn visit_infix_expression(&mut self, node: &ASTNode, builder: &mut FunctionBuilder) -> miette::Result<ir::Value> {
        let (left, op, right) = match node.node_type() {
            ASTNodeType::InfixExpression { left, op, right } => (left, op, right),
            _ => unreachable!()
        };

        let (left_side, right_side) = self.ensure_share_types(&left, &right, builder)?;

        let out: ir::Value;
        match op.token_type() {
            TokenType::Plus => {
                out = builder.ins().iadd(left_side, right_side);
            },
            TokenType::Minus => {
                out = builder.ins().isub(left_side, right_side);
            },
            TokenType::Times => {
                out = builder.ins().imul(left_side, right_side);
            },

            TokenType::EqEq => {
                out = builder.ins().icmp(IntCC::Equal, left_side, right_side);
            },
            TokenType::NotEq => {
                out = builder.ins().icmp(IntCC::NotEqual, left_side, right_side);
            },
            TokenType::Lt => {
                out = builder.ins().icmp(IntCC::SignedLessThan, left_side, right_side);
            },
            TokenType::Gt => {
                out = builder.ins().icmp(IntCC::SignedGreaterThan, left_side, right_side);
            },
            TokenType::LtEq => {
                out = builder.ins().icmp(IntCC::SignedLessThanOrEqual, left_side, right_side);
            },
            TokenType::GtEq => {
                out = builder.ins().icmp(IntCC::SignedGreaterThanOrEqual, left_side, right_side);
            },

            _ => {
                return Err(InvalidOperator {
                    op: op.token_type().clone(),
                    span: op.source_span(),
                    src: self.named_source(),
                    value: String::from("for infix expression")
                }.into());
            }
        }

        Ok(out)
    }

    fn visit_get_field(&mut self, node: &ASTNode, builder: &mut FunctionBuilder) -> miette::Result<ir::Value> {
        let (left_node, op, right_node) = match node.node_type() {
            ASTNodeType::InfixExpression { left, op, right } => (left, op, right),
            _ => unreachable!()
        };

        let left = self.visit_value(left_node, builder)?;
        let struct_type = self.resolve_type(left_node)?;
        let comet_struct = match struct_type.kind {
            CometTypeKind::Struct(value) => value,
            _ => {  return Err(TypeMismatch {
                        src: self.named_source(),
                        expected: String::from("struct"),
                        invalid: struct_type.cranelift_type.to_string(),
                        span: left_node.source_span()
                    }.into()); }
        };

        let field_name = match right_node.node_type() {
            ASTNodeType::IdentifierLiteral(value) => value,
            _ => { return Err(SyntaxError {
                src: self.named_source(),
                text: format!("Expected identifier after '{:?}', got '{:?}' instead", op.token_type(), right_node.node_type()),
                span: right_node.source_span()
            }.into()); }
        };

        let field_index = comet_struct.get_field_index(field_name).ok_or_else(|| UnkownField {
            struct_name: String::from(comet_struct.name()),
            field: field_name.clone(),
            span: right_node.source_span(),
            src: self.named_source()
        })?;
        let field = comet_struct.get_field(field_name).unwrap();

        let comet_struct_layout = comet_struct.get_layout();
                        
        Ok(builder.ins().load(field.field_type().cranelift_type, MemFlagsData::new(), left, comet_struct_layout.0[*field_index] as i32))
    }

    fn visit_expression_statement(&mut self, node: &ASTNode, builder: &mut FunctionBuilder) -> miette::Result<()> {
        match node.node_type() {
            ASTNodeType::ExpressionStatement(expr) => {
                self.visit_value(expr, builder)?;
            },
            _ => unreachable!()
        }

        Ok(())
    }

    fn visit_func_def(&mut self, node: &'a ASTNode) -> miette::Result<()> {

        let mut sig = self.module.make_signature();

        let (name_node, func_args, return_type_optional, body) = match node.node_type() {
            ASTNodeType::FuncDefinitionStatement {name, args, return_type, body } => (name, args, return_type, body),
            _ => unreachable!()
        };

        let name = match name_node.node_type() {
            ASTNodeType::IdentifierLiteral(value) => value,
            _ => unreachable!()
        };

        let mut builder_context = FunctionBuilderContext::new();

        for (i, arg) in func_args.iter().enumerate() {
            let (arg_name_node, arg_type_node)= match arg.node_type() {
                ASTNodeType::FuncArgDefinition { name, type_ } => (name, type_),
                _ => unreachable!()
            };

            let arg_type = self.get_type_literal_type(arg_type_node)?.clone();

            let arg_name = match arg_name_node.node_type() {
                ASTNodeType::IdentifierLiteral(name) => name,
                _ => unreachable!()
            }; 

            sig.params.push(AbiParam::new(arg_type.cranelift_type));
            self.scopes.last_mut().unwrap().variables.insert(&arg_name, CometVariable {
                type_: arg_type,
                var_type: CometVarType::FuncArg(i),
                mutable: false,
                function_id: None
            });
        }

        if return_type_optional.is_some() {
            let return_type_node = return_type_optional.as_ref().unwrap();
            let return_type = self.get_type_literal_type(&return_type_node)?;

            sig.returns.push(AbiParam::new(return_type.cranelift_type));
            
        }

        let func_id = self.module.declare_function(&name, Linkage::Export, &sig).unwrap();

        let mut ctx = self.module.make_context();
        ctx.func.signature = sig;
        ctx.func.name = ir::UserFuncName::user(0, func_id.as_u32());

        // build body of function //
        let mut builder = FunctionBuilder::new(&mut ctx.func, &mut builder_context);

        let entry_block = builder.create_block();
        builder.append_block_params_for_function_params(entry_block);
        builder.switch_to_block(entry_block);

        let target_config = self.module.isa().frontend_config();

        let func_ref = self.module.declare_func_in_func(func_id, builder.func);
        let func_addr = builder.ins().func_addr(target_config.pointer_type(), func_ref);

        let func_var = Variable::from_u32(self.var_index);
        builder.declare_var(target_config.pointer_type());
        builder.def_var(func_var, func_addr);

        self.scopes.last_mut().unwrap().variables.insert(&name, CometVariable {
            type_: CometType::new(target_config.pointer_type()),
            var_type: Local(func_var),
            mutable: false,
            function_id: Some(func_id)
        });
        self.scopes.push(ScopeFrame::new());

        // generate code
        let result = (|| {
            self.compile(body, Some(&mut builder))
        })();

        self.scopes.pop();
        result?;

        builder.seal_all_blocks();

        
        // finalize func
        builder.finalize(target_config);

       

        println!("=== BUILT FUNCTION ===\n{}", ctx.func);

        self.module.define_function(func_id, &mut ctx).unwrap();
        self.module.clear_context(&mut ctx);

        Ok(())
    }

    fn visit_ret_statement(&mut self, node: &ASTNode, builder: &mut FunctionBuilder) -> miette::Result<()> {
        let ret_value_node = match node.node_type() {
            ASTNodeType::ReturnStatement(value) => value,
            _ => unreachable!()
        };

        if ret_value_node.is_none() {
            builder.ins().return_(&[]);
        } else {
            let ret_value = self.visit_value(ret_value_node.as_ref().unwrap(), builder)?;
            builder.ins().return_(&[ret_value]);
        }

        Ok(())
    }

    fn visit_assign_statement(&mut self, node: &'a ASTNode, builder: &mut FunctionBuilder) -> miette::Result<()> {
        let (ident_node, type_node, value_node) = match node.node_type() {
            ASTNodeType::AssignStatement { ident, type_, value } => (ident, type_, value),
            _ => unreachable!()
        };

        // reassignment of struct field
        if !matches!(ident_node.node_type(), ASTNodeType::IdentifierLiteral(_)) {
            if type_node.is_some() {
                return Err(SyntaxError {
                    span: type_node.as_ref().unwrap().source_span(),
                    src: self.named_source(),
                    text: String::from("Cannot type annotate a field assignment")
                }.into());
            }

            let target_type = self.resolve_type(ident_node)?;
            let value_type = self.resolve_type(value_node)?;
            if target_type != value_type {
                CometType::try_implicit_cast(value_node, self.visit_value(value_node, builder)?, &value_type, &target_type, builder, self.named_source())?;
            }

            let value = self.visit_value(value_node, builder)?;
            let address = self.visit_l_value(ident_node, builder)?;
            builder.ins().store(MemFlagsData::new(), value, address, 0);
            return Ok(());
        }

        let ident = match ident_node.node_type() {
            ASTNodeType::IdentifierLiteral(value) => value,
            _ => unreachable!()
        };

        // get type of the value we're setting the variable to
        let value_type = self.resolve_type(value_node)?;

        // build variable value
        let mut value = self.visit_value(value_node, builder)?;

        let existing_var = self.get_variable(&ident.as_str());
        if existing_var.is_some() {
            if !existing_var.unwrap().mutable {
                return Err(ImmutableReassignment {
                    span: ident_node.source_span(),
                    src: self.named_source(),
                    var_name: ident.clone()
                }.into());
            }

            let var_type = &existing_var.unwrap().type_;
            if value_type != *var_type {
                return Err(TypeMismatch {
                    expected: var_type.cranelift_type.to_string(),
                    invalid: value_type.cranelift_type.to_string(),
                    span: value_node.source_span(),
                    src: self.named_source()
                }.into());
            }

            if type_node.is_some() {
                return Err(SyntaxError {
                    span: type_node.as_ref().unwrap().source_span(),
                    src: self.named_source(),
                    text: String::from("Cannot type annotate reassignment")
                }.into());
            }

            match existing_var.unwrap().var_type {
                CometVarType::Local(var) => { builder.def_var(var, value); },
                _ => unreachable!()
            }

            return Ok(());
        }

        let new_variable = Variable::from_u32(self.var_index);
        self.var_index += 1;
        
        let mut final_type = value_type.clone();
        if type_node.is_some() {
            let var_type = self.get_type_literal_type(type_node.as_ref().unwrap())?;

            // get type of type annotation
            if var_type != value_type {
                value = CometType::try_implicit_cast(&value_node, value, &value_type, &var_type, builder, self.named_source())?;
                final_type = var_type;
            } else {
                final_type = self.unify_types(&final_type, &var_type).clone();
            }
        }

        builder.declare_var(final_type.cranelift_type);
        builder.def_var(new_variable, value);

        let function_id = match value_node.node_type() {
            ASTNodeType::IdentifierLiteral(name) => self.get_variable(name)
                .and_then(|variable| variable.function_id),
            _ => None
        };

        let comet_var = CometVariable {
            type_: final_type,
            var_type: CometVarType::Local(new_variable),
            mutable: true,
            function_id
        };

        self.scopes.last_mut().unwrap().variables.insert(ident.as_str(), comet_var);

        Ok(())
    }

    fn visit_match_statement(&mut self, node: &'a ASTNode, builder: &mut FunctionBuilder) -> miette::Result<()> {
        let (expr_node, match_nodes, default_branch) = match node.node_type() {
            ASTNodeType::MatchStatement { expr, nodes, default } => (expr, nodes, default),
            _ => unreachable!()
        };

        let expr = self.visit_value(expr_node, builder)?;
        let end_block = builder.create_block();
        let default_block = default_branch.as_ref().map(|_| builder.create_block()); // only create a block if the default_branch exists

        let mut compare_block = builder.current_block().unwrap();

        for (arm_index, match_node) in match_nodes.iter().enumerate() {

            let (match_expr_nodes, match_block) = match match_node.node_type() {
                ASTNodeType::MatchNode { expressions, block } => (expressions, block),
                _ => unreachable!()
            };

            let arm_block = builder.create_block();

            for (expr_index, match_expr_node) in match_expr_nodes.iter().enumerate() {
                if builder.current_block() != Some(compare_block) {
                    builder.switch_to_block(compare_block);
                }

                let match_expr = self.visit_value(match_expr_node, builder)?;
                let is_equal = builder.ins().icmp(IntCC::Equal, expr, match_expr);

                let false_block = if expr_index + 1 < match_expr_nodes.len() {
                    builder.create_block()
                } else if arm_index + 1 < match_nodes.len() {
                    builder.create_block()
                } else {
                    default_block.unwrap_or(end_block)
                };

                builder.ins().brif(is_equal, arm_block, &[], false_block, &[]);
                builder.seal_block(compare_block);
                compare_block = false_block;
            }

            builder.switch_to_block(arm_block);


            self.compile(match_block, Some(builder))?;

            let arm_is_terminated = self.block_is_terminated(arm_block, builder);
            if !arm_is_terminated {
                builder.ins().jump(end_block, &[]);
            }

            builder.seal_block(arm_block);
        }

        if let Some(default_branch) = default_branch {
            let default_block = default_block.unwrap();
            builder.switch_to_block(default_block);

            self.compile(default_branch, Some(builder))?;

            let default_is_terminated = self.block_is_terminated(default_block, builder);
            if !default_is_terminated {
                builder.ins().jump(end_block, &[]);
            }

            builder.seal_block(default_block);
        }

        builder.switch_to_block(end_block);
        builder.ensure_inserted_block();

        Ok(())
    }

    fn block_is_terminated(&self, block: Block, builder: &mut FunctionBuilder) -> bool {
        let is_terminated = builder
                    .func
                    .layout
                    .last_inst(block)
                    .map(|inst| builder.func.dfg.insts[inst].opcode().is_terminator())
                    .unwrap_or(false);

        return is_terminated;
    }

    fn visit_if_statement(&mut self, node: &'a ASTNode, builder: &mut FunctionBuilder) -> miette::Result<()> {
        let (expr_node, body_node, else_body) = match node.node_type() {
            ASTNodeType::IfStatement { expr, body, else_body } => (expr, body, else_body),
            _ => unreachable!()
        };

        let expr = self.visit_value(expr_node, builder)?;

        let then_block = builder.create_block();
        let else_block = builder.create_block();
        let end_block = else_body.as_ref().map(|_| builder.create_block()).unwrap_or(else_block);

        builder.ins().brif(expr, then_block, &[], else_block, &[]);
        builder.seal_block(builder.current_block().unwrap());

        builder.switch_to_block(then_block);
        builder.ensure_inserted_block();
        self.compile(body_node, Some(builder))?;

        let then_block_terminated = self.block_is_terminated(then_block, builder);
        if !then_block_terminated {
            builder.ins().jump(end_block, &[]);
        }

        builder.seal_block(then_block);

        builder.switch_to_block(else_block);
        if else_body.is_some() {
            self.compile(else_body.as_ref().unwrap(), Some(builder))?;

            let else_block_terminated = self.block_is_terminated(else_block, builder);
            if !else_block_terminated {
                builder.ins().jump(end_block, &[]);
            }

            builder.seal_block(else_block);
            builder.switch_to_block(end_block);
            builder.ensure_inserted_block();
        }

        Ok(())
    }

    fn visit_while_statement(&mut self, node: &'a ASTNode, builder: &mut FunctionBuilder) -> miette::Result<()> {
        let (expr_node, body_node) = match node.node_type() {
            ASTNodeType::WhileStatement { expr, body } => (expr, body),
            _ => unreachable!()
        };

        let compare_block = builder.create_block();
        let body_block = builder.create_block();
        let end_block = builder.create_block();

        builder.ins().jump(compare_block, &[]);
        builder.seal_block(builder.current_block().unwrap());
        builder.switch_to_block(compare_block);
        let expr = self.visit_value(expr_node, builder)?;
        builder.ins().brif(expr, body_block, &[], end_block, &[]);

        builder.switch_to_block(body_block);

        self.compile(body_node, Some(builder))?;

        builder.ins().jump(compare_block, &[]);
        builder.seal_block(compare_block);

        builder.seal_block(body_block);
        builder.switch_to_block(end_block);

        Ok(())
    }

    fn visit_struct_def_statement(&mut self, node: &'a ASTNode) -> miette::Result<()> {
        let (ident_node, field_nodes) = match node.node_type() {
            ASTNodeType::StructDefinitionStatement { ident, fields } => (ident, fields),
            _ => unreachable!()
        };

        let ident = match ident_node.node_type() {
            ASTNodeType::IdentifierLiteral(value) => value,
            _ => unreachable!()
        };

        let mut fields: Vec<CometStructField> = Vec::new();

        for field_node in field_nodes {
            let (field_ident_node, field_type_node) = match field_node.node_type() {
                ASTNodeType::StructFieldDefinition { ident, type_ } => (ident, type_),
                _ => unreachable!()
            };

            let field_name = match field_ident_node.node_type() {
                ASTNodeType::IdentifierLiteral(value) => value,
                _ => unreachable!()
            };

            let field_type = self.get_type_literal_type(field_type_node)?;

            fields.push(CometStructField::new(field_name.clone(), field_type.clone()));
        }

        let new_struct = CometStruct::new(ident.clone(), fields);
        self.scopes.last_mut().unwrap().types.insert(&ident, CometType::new_struct(new_struct));

        Ok(())
    }

    fn visit_extern_directive(&mut self, name_node: &'a ASTNode, type_node: &'a ASTNode) -> miette::Result<()> {
        let name = match name_node.node_type() {
            ASTNodeType::IdentifierLiteral(value) => value,
            _ => unreachable!()
        };
        
        // make signature
        let mut sig = self.module.make_signature();
        let comet_type = self.get_type_literal_type(type_node)?;

        let func_type = match comet_type.kind {
            CometTypeKind::Function(v) => v,
            _ => { return Err(CompilerBug {
                src: self.named_source(),
                span: type_node.source_span(),
                text: String::from("only functions are supported with #extern")
            }.into()); }
        };

        sig.returns.push(AbiParam::new(func_type.return_type.cranelift_type));
        for arg in func_type.arg_types {
            sig.params.push(AbiParam::new(arg.cranelift_type))
        }

        // make external func with new sig
        let ext_func_id = self.module
            .declare_function(name, Linkage::Import, &sig)
            .unwrap();

        let data_id = self.module
            .declare_data(&format!("{}_var", name), Linkage::Local, true, false)
            .unwrap();

        let mut data_desc = DataDescription::new();
        let target_config = self.module.isa().frontend_config();
        let pointer_size = target_config.pointer_type().bytes() as usize;

        data_desc.define_zeroinit(pointer_size);

        let data_func_ref = self.module.declare_func_in_data(ext_func_id, &mut data_desc);
        data_desc.write_function_addr(0, data_func_ref);

        self.module.define_data(data_id, &data_desc).unwrap();

        self.scopes.last_mut().unwrap().variables.insert(&name, CometVariable {
            type_: CometType::new(target_config.pointer_type()),
            var_type: External(data_id),
            mutable: false,
            function_id: Some(ext_func_id)
        });

        Ok(())
    }

    fn visit_compiler_directive(&mut self, node: &'a ASTNode) -> miette::Result<()> {
        let (directive_node, name_node, type_node) = match node.node_type() {
            ASTNodeType::CompilerDirectiveStatement { directive, value_name, value_type } => (directive, value_name, value_type),
            _ => unreachable!()
        };

        let directive = match directive_node.node_type() {
            ASTNodeType::IdentifierLiteral(v) => v,
            _ => unreachable!()
        };

        match directive.as_str() {
            "extern" => {
                self.visit_extern_directive(name_node, type_node)
            },
            _ => Err(InvalidCompilerDirective {
                directive: directive.clone(),
                span: directive_node.source_span(),
                src: self.named_source()
            }.into())
        }
        
    }
    // END OF VISIT METHODS //

    pub fn compile(&mut self, ast: &'a ASTNode, builder: Option<&mut FunctionBuilder>) -> miette::Result<()> {
        match ast.node_type() {
            ASTNodeType::Program(_) => { return self.visit_program(ast); },
            ASTNodeType::Block(_) => { return self.visit_block(ast, builder.unwrap()); },

            ASTNodeType::FuncDefinitionStatement {name: _, args: _, return_type: _, body: _ } => { return self.visit_func_def(ast); },
            ASTNodeType::ExpressionStatement(_) => { return self.visit_expression_statement(ast, builder.unwrap()); },
            ASTNodeType::ReturnStatement(_) => { return self.visit_ret_statement(ast, builder.unwrap()); }
            ASTNodeType::AssignStatement { ident: _, type_: _, value: _ } => { return self.visit_assign_statement(ast, builder.unwrap()); }
            ASTNodeType::MatchStatement { expr: _, nodes: _, default: _ } => { return self.visit_match_statement(ast, builder.unwrap()); },
            ASTNodeType::IfStatement { expr: _, body: _, else_body: _ } => { return self.visit_if_statement(ast, builder.unwrap()) },
            ASTNodeType::WhileStatement { expr: _, body: _ } => { return self.visit_while_statement(ast, builder.unwrap()); },
            ASTNodeType::StructDefinitionStatement { ident: _, fields: _ } => { return self.visit_struct_def_statement(ast); },

            ASTNodeType::CompilerDirectiveStatement { directive: _, value_name: _, value_type: _ } => { return self.visit_compiler_directive(ast); },

            _ => {
                Err(CompilerBug {
                    span: ast.source_span(),
                    src: self.named_source(),
                    text: format!("No compiler visit method for {:?}", ast.node_type())
                }.into())
            }
        }
    }
}