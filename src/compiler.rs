use cranelift_codegen::ir::condcodes::{FloatCC, IntCC};
use cranelift_codegen::{ir::AbiParam, settings};
use cranelift_codegen::ir::{self, Block, InstBuilder, MemFlagsData, StackSlotData, StackSlotKind, types};
use cranelift_frontend::{FunctionBuilder, FunctionBuilderContext};
use cranelift_module::{DataDescription, FuncId, Linkage, Module, default_libcall_names};
use cranelift_native::builder;
use cranelift_object::{ObjectBuilder, ObjectModule};
use miette::NamedSource;
use std::collections::HashMap;

use crate::ast::ASTType;
use crate::comet_error::{CompilerBug, EmptyArrayLiteral, ImmutableReassignment, InvalidCompilerDirective, InvalidLValue, InvalidOperator, InvalidVariableType, NotAFunction, SyntaxError, TypeAnnotationNeeded, TypeMismatch, UndefinedVariable, UnkownField, UnkownMethod, UnkownType};
use crate::comet_struct::{CometStruct, CometStructField};
use crate::comet_type::{CometFunction, CometMethod, CometTypeKind, FunctionOwner};
use crate::scope::CometVarType::Local;
use crate::scope::{CometVarType, CometVariable, ScopeFrame};
use crate::{ast::{ASTNode, ASTNodeType}, comet_type::CometType, token::TokenType};

pub struct Compiler <'a> {
    module: ObjectModule,

    scopes: Vec<ScopeFrame<'a>>,
    methods: HashMap<(String, String), CometMethod>,
    resolved_generics: HashMap<String, CometStruct>,

    generic_impls: Vec<ASTNode<'a>>,

    current_function: Option<CometFunction>,

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

        // define all inbuilt types
        base_frame.types.insert("bool".to_string(), CometType::new(types::I8));
        base_frame.types.insert("i8".to_string(), CometType::new_int(types::I8, true));
        base_frame.types.insert("i16".to_string(), CometType::new_int(types::I16, true));
        base_frame.types.insert("i32".to_string(), CometType::new_int(types::I32, true));
        base_frame.types.insert("i64".to_string(), CometType::new_int(types::I64, true));
        base_frame.types.insert("isize".to_string(), CometType::new_int(module.isa().pointer_type(), true));
        base_frame.types.insert("u8".to_string(), CometType::new_int(types::I8, false));
        base_frame.types.insert("u16".to_string(), CometType::new_int(types::I16, false));
        base_frame.types.insert("u32".to_string(), CometType::new_int(types::I32, false));
        base_frame.types.insert("u64".to_string(), CometType::new_int(types::I64, false));
        base_frame.types.insert("usize".to_string(), CometType::new_int(module.isa().pointer_type(), false));
        base_frame.types.insert("f16".to_string(), CometType::new(types::F16));
        base_frame.types.insert("f32".to_string(), CometType::new(types::F32));
        base_frame.types.insert("f64".to_string(), CometType::new(types::F64));
        base_frame.types.insert("str".to_string(), CometType::new_ptr(CometType::new_int(types::I8, true), module.isa().pointer_type()));
        

        Ok(Compiler {
            module: module,
            scopes: vec!{base_frame},
            file_name: file_name,
            source: source,
            current_function: None,

            methods: HashMap::new(),
            resolved_generics: HashMap::new(),
            generic_impls: Vec::new(),

            var_index: 0
        })
    }

    pub fn end_module(self) -> miette::Result<Vec<u8>, cranelift_object::object::write::Error> {
        /* Compile finished program. */
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

    fn get_method(&self, struct_name: String, method_name: String) -> Option<&CometMethod> {
        self.methods.get(&(struct_name, method_name))
    }

    fn get_generic(&self, name: &str) -> Option<&ASTNode<'a>> {
        self.scopes
            .iter()
            .rev()
            .find_map(|scope| scope.generics.get(name))
    }

    fn instantiate_generic_impls(
        &mut self,
        base_name: &str,
        concrete_types: &[CometType],
        concrete_struct: &CometStruct
    ) -> miette::Result<()> {

        let impls = self.generic_impls.clone();

        for impl_node in impls {
            let (struct_type_node, functions) = match impl_node.node_type() {
                ASTNodeType::ImplDefStatement { struct_type, functions } => (struct_type, functions),
                _ => unreachable!()
            };

            let target = match struct_type_node.node_type() {
                ASTNodeType::TypeLiteral { base_type, generic_types, .. } => (base_type, generic_types),
                _ => continue,
            };

            let type_name = match target.0 {
                ASTType::Identifier(name_node) => match name_node.as_ref().node_type() {
                    ASTNodeType::IdentifierLiteral(name) => name,
                    _ => continue,
                },
                _ => continue,
            };

            if type_name != base_name {
                continue;
            }

            
            let generic_names = match target.1.as_ref() {
                Some(generic_nodes) => generic_nodes
                    .iter()
                    .map(|n| match n.node_type() {
                        ASTNodeType::TypeLiteral {
                            base_type: ASTType::Identifier(name_node),
                            generic_types: None,
                        } => match name_node.node_type() {
                            ASTNodeType::IdentifierLiteral(name) => Ok(name.clone()),
                            _ => Err(()),
                        },
                        _ => Err(()),
                    })
                    .collect::<Result<Vec<_>, _>>(),
                None => continue,
            };

            let generic_names = match generic_names {
                Ok(names) => names,
                Err(()) => continue,
            };

            if generic_names.len() != concrete_types.len() {
                continue;
            }

            self.scopes.push(ScopeFrame::new());

            for (generic_name, concrete_type) in generic_names.iter().zip(concrete_types.iter()) {
                self.scopes.last_mut().unwrap().types.insert(
                    generic_name.clone(),
                    concrete_type.clone(),
                );
            }

            self.scopes.last_mut().unwrap().types.insert(
                "Self".to_string(),
                CometType::new_struct(concrete_struct.clone()),
            );

            for function in functions {
                let (name_node, args, return_type, body) = match function.node_type() {
                    ASTNodeType::FuncDefinitionStatement { name, args, return_type, body } => {
                        (name, args, return_type, body)
                    }
                    _ => unreachable!()
                };

                let method_name = match name_node.node_type() {
                    ASTNodeType::IdentifierLiteral(v) => v,
                    _ => unreachable!()
                };

                let func_id = self.compile_function(
                    method_name,
                    args,
                    return_type.as_ref().map(|r| r.as_ref()),
                    body,
                    FunctionOwner::Impl(concrete_struct.name().to_string()),
                )?;

                let mut arg_types = Vec::new();
                for func_arg in args {
                    let arg_type = match func_arg.node_type() {
                        ASTNodeType::FuncArgDefinition { type_, .. } => self.get_type_literal_type(type_)?,
                        _ => unreachable!()
                    };
                    arg_types.push(arg_type);
                }

                let mut compiled_return = CometType::new_void();

                if let Some(t) = return_type.as_ref() {
                    compiled_return = self.get_type_literal_type(t)?;
                }

                let function = CometFunction {
                    arg_types,
                    return_type: Box::new(compiled_return),
                };

                self.methods.insert(
                    (concrete_struct.name().to_string(), method_name.clone()),
                    CometMethod { function, func_id },
                );
            }

            self.scopes.pop();
        }

        Ok(())
    }

    fn get_generic_type_literal_type<'b>(
        &mut self,
        ast_type: &ASTType<'b>,
        ast_type_node: &ASTNode,
        generic_types: &[ASTNode<'b>]
    ) -> miette::Result<CometType> {
        let struct_name_node = match ast_type {
            ASTType::Identifier(struct_name_node) => struct_name_node,
            _ => { return Err(SyntaxError {
                span: ast_type_node.source_span(),
                src: self.named_source(),
                text: "expected identifier".to_string()
            }.into()) }
        };

        let struct_name = match struct_name_node.node_type() {
            ASTNodeType::IdentifierLiteral(struct_name) => struct_name,
            _ => unreachable!()
        };

        let mut struct_template = self.get_generic(struct_name).ok_or_else(|| UnkownType {
            span: ast_type_node.source_span(),
            src: self.named_source(),
            type_: struct_name.clone()
        })?.clone();

        let generic_names: Vec<String> = match struct_template.node_type_mut() {
            ASTNodeType::StructDefinitionStatement { ident: _, fields: _, generics } => { 
                let generic_names = generics
                .as_ref()
                .unwrap()
                .iter()
                .map(|generic_node| match generic_node.node_type() {
                    ASTNodeType::IdentifierLiteral(v) => v.clone(),
                    _ => unreachable!()
                })
                .collect();

                *generics = None;

                generic_names
            }
            _ => unreachable!()
        };

        self.scopes.push(ScopeFrame::new());

        let resolved_generic_types: Vec<CometType> = generic_types
            .iter()
            .map(|t| self.get_type_literal_type(t))
            .collect::<miette::Result<Vec<_>>>()?;

        let existing_struct_name = CometStruct::get_mangled_name(struct_name, &resolved_generic_types);
        let existing_struct = self.resolved_generics.get(&existing_struct_name);
        if existing_struct.is_some() {
            return Ok(CometType::new_struct(existing_struct.unwrap().clone()));
        }

        for (i, generic_name) in generic_names.iter().enumerate() {
            self.scopes.last_mut().unwrap().types.insert(
                generic_name.clone(),
                resolved_generic_types[i].clone()
            );
        }


        let mut struct_result = self.visit_struct_def_statement(&struct_template)?.unwrap();
        struct_result.mangle_name(&resolved_generic_types);

        self.resolved_generics.insert(struct_result.name().to_string(), struct_result.clone());

        self.instantiate_generic_impls(struct_name, &resolved_generic_types, &struct_result)?;

        self.scopes.pop();

        Ok(CometType::new_struct(struct_result))
    }

    fn get_type_literal_type<'b> (&mut self, node: &ASTNode<'b>) -> miette::Result<CometType> {
        /* Takes in a type literal node and returns the type it represents. */

        let (ast_type, generic_types) = match node.node_type() {
            ASTNodeType::TypeLiteral{ base_type, generic_types } => (base_type, generic_types),
            _ => unreachable!()
        };

        if generic_types.is_some() {
            return self.get_generic_type_literal_type(ast_type, node, generic_types.as_ref().unwrap());
        }

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
            },

            ASTType::Pointer(inner_node) => {
                let inner_type = self.get_type_literal_type(inner_node)?;
                Ok(CometType::new_ptr(inner_type, self.module.isa().pointer_type()))
            }

            ASTType::Array(inner_node, length_node) => {
                let inner_type = self.get_type_literal_type(inner_node)?;

                let length = match length_node.node_type() {
                    ASTNodeType::IntLiteral(n) => *n,
                    _ => unreachable!()
                };

                Ok(CometType::new_array(inner_type, length as u32, self.module.isa().pointer_type()))
            }
        }
        
    }

    fn rank_type(&self, type_: &CometType) -> u8 {
        match type_.cranelift_type {
            types::INVALID => 0,
            types::I8 => 1,
            types::I16 => 2,
            types::I32 => 3,
            types::I64 => 4,
            types::F16 => 5,
            types::F32 => 6,
            types::F64 => 7,
            _ => 0
        }
    }

    fn unify_types(&self, a: &'a CometType, b: &'a CometType) -> &CometType {
        if self.rank_type(a) >= 4 || self.rank_type(b) >= 4 {
            return if self.rank_type(a) > self.rank_type(b) {
                a
            } else {
                b
            };
        }

        if !a.is_int() || !b.is_int() {
            return if self.rank_type(a) > self.rank_type(b) {
                a
            } else {
                b
            };
        }

        let a_bits = a.cranelift_type.bits();
        let b_bits = b.cranelift_type.bits();

        let a_signed = a.is_signed();
        let b_signed = b.is_signed();

        if a_signed == b_signed {
            return if a_bits >= b_bits { a } else { b };
        }

        let (signed, unsigned) = if a_signed {
            (a, b)
        } else {
            (b, a)
        };

        let signed_bits = signed.cranelift_type.bits();
        let unsigned_bits = unsigned.cranelift_type.bits();

        if signed_bits > unsigned_bits {
            // e.g. i64 + u32 -> i64
            signed
        } else {
            // e.g. i32 + u32 -> u32
            //      i64 + u64 -> u64
            //      i32 + u64 -> u64
            unsigned
        }
    }

    fn resolve_type(&mut self, node: &ASTNode) -> miette::Result<CometType> {
        match node.node_type() {
            ASTNodeType::IntLiteral(_) => {
                Ok(CometType::new_int(types::I64, false))
            },

            ASTNodeType::FloatLiteral(_) => {
                Ok(CometType::new(types::F64))
            },

            ASTNodeType::StringLiteral(_) => {
                self.get_type("str").cloned().ok_or_else(|| CompilerBug {
                    span: node.source_span(),
                    src: self.named_source(),
                    text: String::from("failed to get internal \"str\" type, this is a bug!")
                }.into())
            },

            ASTNodeType::IdentifierLiteral(name) => {
                let var = self.get_variable(&name.as_str()).ok_or(UndefinedVariable {
                    span: node.source_span(),
                    src: self.named_source(),
                    var: name.clone()
                })?;

                Ok(var.type_.clone())
            },

            ASTNodeType::ArrayLiteral(elems) => {
                if elems.len() == 0 {
                    return Ok(CometType::new_unkown());
                }

                let base_elem_type = self.resolve_type(&elems[0])?;
                Ok(CometType::new_array(base_elem_type, elems.len() as u32, self.module.isa().pointer_type()))
            }

            ASTNodeType::IndexExpression { left, .. } => {
                let left_type = self.resolve_type(left)?;

                let elem_type = match left_type.kind {
                    CometTypeKind::Array { base_type, .. } => base_type,
                    _ => { return Err(TypeMismatch {
                        expected: "array".to_string(),
                        invalid: left_type.to_string(),
                        span: left.source_span(),
                        src: self.named_source()
                    }.into()); }
                };

                Ok(*elem_type)
            },

            ASTNodeType::InfixExpression { left, op, right } => {
                let left_value = self.resolve_type(left)?;

                match op.token_type() {
                    TokenType::Dot | TokenType::ColonColon => {
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

                        let method = self.get_method(struct_type.name().to_string(), field_name.to_string());
                        if method.is_some() { // getting method
                            let method = method.unwrap();
                            return Ok(CometType::new_function(method.function.clone()));
                        }

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

                if op.token_type() == &TokenType::Or || op.token_type() == &TokenType::And {
                    return Ok(self.unify_types(&left_value, &right_value).clone());
                }

                return Ok(self.unify_types(&left_value, &right_value).clone());
            },

            ASTNodeType::PrefixExpression { op, right } => {
                let right_type = self.resolve_type(right)?;

                match op.token_type() {
                    TokenType::Ampersand => {
                        Ok(CometType::new_ptr(right_type, self.module.isa().pointer_type()))
                    },
                    TokenType::Times => {
                        let inner_type = match right_type.kind {
                            CometTypeKind::Pointer(t) => *t,
                            _ => { return Err(TypeMismatch {
                                src: self.named_source(),
                                span: right.source_span(),
                                invalid: right_type.to_string(),
                                expected: "pointer".to_string()
                            }.into()); }
                        };

                        Ok(inner_type)
                    },

                    TokenType::Minus => {
                        let mut new_type = right_type.clone();                        
                        new_type.negate();
                        Ok(new_type)
                    }

                    TokenType::Tilde => {
                        if !right_type.is_int() {
                            return Err(InvalidOperator {
                                op: op.token_type().clone(),
                                span: op.source_span(),
                                src: self.named_source(),
                                value: "on non-integer".to_string()
                            }.into());
                        }

                        Ok(right_type)
                    }

                    TokenType::Not => {
                        self.get_type("bool").cloned().ok_or_else(|| CompilerBug {
                            span: node.source_span(),
                            src: self.named_source(),
                            text: String::from("failed to get internal \"bool\" type, this is a bug!")
                        }.into())
                    }

                    _ => Err(InvalidOperator {
                        op: op.token_type().clone(),
                        span: op.source_span(),
                        src: self.named_source(),
                        value: "as a prefix operator".to_string()
                    }.into())
                }
            },

            ASTNodeType::FuncCall { left, args: _ } => {
                let function_type = self.resolve_type(left)?;

                match function_type.kind {
                    CometTypeKind::Function(function) => Ok((*function.return_type).clone()),
                    _ => Err(NotAFunction {
                        span: left.source_span(),
                        src: self.named_source(),
                        func_name: match left.node_type() {
                            ASTNodeType::IdentifierLiteral(name) => name.clone(),
                            _ => String::from("<expression>"),
                        }
                    }.into())
                }
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
    fn visit_program(&mut self, node: &ASTNode<'a>) -> miette::Result<()> {
        let nodes = match node.node_type() {
            ASTNodeType::Program(value) => value,
            _ => unreachable!()
        };

        for node in nodes {
            self.compile(node, None)?;
        }

        Ok(())
    }

    fn visit_block(&mut self, node: &ASTNode<'a>, builder: &mut FunctionBuilder) -> miette::Result<()> {
        let nodes = match node.node_type() {
            ASTNodeType::Block(value) => value,
            _ => unreachable!()
        };

        for node in nodes {
            self.compile(node, Some(builder))?;
        }

        Ok(())
    }

    fn visit_method_call(
        &mut self,
        receiver_node: &ASTNode<'a>,
        method_node: &ASTNode<'a>,
        op: &TokenType,
        args: &[ASTNode<'a>],
        builder: &mut FunctionBuilder
    ) -> miette::Result<ir::Value> {
        let receiver_type = self.resolve_type(receiver_node)?;

        let struct_name = match &receiver_type.kind {
            CometTypeKind::Struct(comet_struct) => comet_struct.name(),
            _ => {
                return Err(TypeMismatch {
                    expected: "struct".to_string(),
                    invalid: receiver_type.to_string(),
                    span: receiver_node.source_span(),
                    src: self.named_source()
                }.into());
            }
        };

        let method_name = match method_node.node_type() {
            ASTNodeType::IdentifierLiteral(name) => name,
            _ => unreachable!(),
        };

        

        let receiver_value = self.visit_value(receiver_node, builder)?;

        let mut compiled_args = vec![];

        if op == &TokenType::Dot {
            compiled_args.push(receiver_value);
        }

        for arg in args {
            compiled_args.push(self.visit_value(arg, builder)?);
        }

        let method = self
            .get_method(struct_name.to_string(), method_name.clone())
            .ok_or(UnkownMethod {
                method: method_name.clone(),
                struct_name: struct_name.to_string(),
                span: method_node.source_span(),
                src: self.named_source()
            })?;

        // method.function.arg_types includes self as argument zero
        let mut sig = self.module.make_signature();

        for arg_type in &method.function.arg_types {
            sig.params.push(AbiParam::new(arg_type.cranelift_type));
        }

        if method.function.return_type.cranelift_type != types::INVALID {
            sig.returns.push(AbiParam::new(
                method.function.return_type.cranelift_type,
            ));
        }

        let method_ref = self
            .module
            .declare_func_in_func(method.func_id, builder.func);

        let call_inst = builder.ins().call(method_ref, &compiled_args);
        let results = builder.inst_results(call_inst);

        if results.is_empty() {
            Ok(builder.ins().iconst(types::I64, 0))
        } else {
            Ok(results[0])
        }
    }

    fn visit_value(&mut self, node: &ASTNode<'a>, builder: &mut FunctionBuilder) -> miette::Result<ir::Value> {
        match node.node_type() {
            ASTNodeType::InfixExpression { left: _, op, right: _ } => {
                if op.token_type() == &TokenType::Dot {
                    self.visit_get_field(node, builder)
                } else {
                    self.visit_infix_expression(node, builder)
                }
            },

            ASTNodeType::PrefixExpression { op: _, right: _ } => self.visit_prefix_expression(node, builder),

            ASTNodeType::IntLiteral(num) => {
                Ok(builder.ins().iconst(types::I64, *num as i64))
            },

            ASTNodeType::FloatLiteral(num) => {
                Ok(builder.ins().f64const(*num as f64))
            },

            ASTNodeType::StringLiteral(str) => {
                let mut data_desc = DataDescription::new();
                data_desc.define(str.as_bytes().to_vec().into_boxed_slice());

                let string_name = format!("string_literal_{}", self.var_index);
                let data_id = self.module
                    .declare_data(&string_name, Linkage::Local, false, false).unwrap();

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
                    CometVarType::External(func_id) => {
                        let func_ref = self.module.declare_func_in_func(func_id, builder.func);
                        let pointer_type = self.module.isa().frontend_config().pointer_type();

                        Ok(builder.ins().func_addr(pointer_type, func_ref))
                    }
                }  
            },

            ASTNodeType::ArrayLiteral(elems) => {
                if elems.len() == 0 {
                    return Err(EmptyArrayLiteral {
                        src: self.named_source(),
                        span: node.source_span()
                    }.into());
                }

                let base_type = match self.resolve_type(node)?.kind {
                    CometTypeKind::Array { base_type, .. } => base_type,
                    _ => unreachable!()
                };

                let stack_slot = builder.create_sized_stack_slot(StackSlotData::new(
                    StackSlotKind::ExplicitSlot,
                    base_type.size() * elems.len() as u32,
                    base_type.align() as u8
                ));

                let stack_addr = builder.ins().stack_addr(self.module.isa().pointer_type(), stack_slot, 0);

                let elem_size = base_type.size() as i32;

                for (i, elem) in elems.iter().enumerate() {
                    let elem_value = self.visit_value(elem, builder)?;

                    builder.ins().store(
                        MemFlagsData::new(),
                        elem_value,
                        stack_addr,
                        (i as i32) * elem_size
                    );
                }

                Ok(stack_addr)
            },

            ASTNodeType::IndexExpression { left, index } => {
                let left_type = self.resolve_type(left)?;

                let elem_type = match left_type.kind {
                    CometTypeKind::Array { base_type, .. } => base_type,
                    _ => { return Err(TypeMismatch {
                        expected: "array".to_string(),
                        invalid: left_type.to_string(),
                        span: left.source_span(),
                        src: self.named_source()
                    }.into()); }
                };

                let array_value = self.visit_value(left, builder)?;
                let index = self.visit_value(index, builder)?;

                let ptr_offset = builder.ins().imul_imm_s(index, elem_type.size() as i64);
                let ptr = builder.ins().iadd(array_value, ptr_offset);

                Ok(builder.ins().load(
                    elem_type.cranelift_type,
                    MemFlagsData::new(),
                    ptr,
                    0
                ))
            },

            ASTNodeType::FuncCall { left, args } => {
                // check if we're calling a method
                if let ASTNodeType::InfixExpression {
                    left,
                    op,
                    right
                } = left.node_type()
                {
                    if op.token_type() == &TokenType::Dot || op.token_type() == &TokenType::ColonColon {
                        return self.visit_method_call(
                            left,
                            right,
                            op.token_type(),
                            args,
                            builder
                        );
                    }
                }


                let func_ptr = self.visit_value(left, builder)?;
                let func_type = self.resolve_type(left)?;

                let comet_function = match func_type.kind {
                    CometTypeKind::Function(f) => f,
                    _ => { return Err(NotAFunction {
                        span: (**left).source_span(),
                        src: NamedSource::new(String::from(self.file_name), self.source.clone()),
                        func_name: String::from("<expression>"),
                    }.into()); }
                };

                let mut compiled_args: Vec<ir::Value> = vec![];
                for arg in args {
                    compiled_args.push(self.visit_value(arg, builder)?);
                }

                let mut sig = self.module.make_signature();
                for arg_type in &comet_function.arg_types {
                    sig.params.push(AbiParam::new(arg_type.cranelift_type));
                }
                if comet_function.return_type.cranelift_type != types::INVALID {
                    sig.returns.push(AbiParam::new(comet_function.return_type.cranelift_type));
                }
                
                let sig_ref = builder.import_signature(sig);
                let call_inst = builder.ins().call_indirect(sig_ref, func_ptr, &compiled_args);
                let results = builder.inst_results(call_inst);


                if results.is_empty() {
                    // Return a dummy value or handle void returns
                    Ok(builder.ins().iconst(types::I64, 0))
                } else {
                    Ok(results[0])
                }
                
            },

            ASTNodeType::StructCreateExpression { type_: type_node, fields } => {
                let struct_type = self.get_type_literal_type(type_node)?;

                let comet_struct = match struct_type.kind {
                    CometTypeKind::Struct(value) => value,

                    _ => { return Err(TypeMismatch {
                        src: self.named_source(),
                        expected: String::from("struct"),
                        invalid: struct_type.to_string(),
                        span: type_node.source_span()
                    }.into()); }
                };

                let struct_layout = comet_struct.get_layout();

                let stack_slot = builder.create_sized_stack_slot(StackSlotData::new( 
                    StackSlotKind::ExplicitSlot,
                    struct_layout.1,
                    8
                ));
                let addr = builder.ins().stack_addr(self.module.isa().pointer_type(), stack_slot, 0);

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

    fn visit_l_value(&mut self, node: &ASTNode<'a>, builder: &mut FunctionBuilder) -> miette::Result<ir::Value> {
        match node.node_type() {
            ASTNodeType::IdentifierLiteral(_) => self.visit_value(node, builder),

            ASTNodeType::PrefixExpression { op, right } => {
                let right_type = self.resolve_type(right)?;
                
                match op.token_type() {
                    TokenType::Times => {
                        match right_type.kind {
                            CometTypeKind::Pointer(_) => {},
                            _ => { return Err(TypeMismatch {
                                src: self.named_source(),
                                span: right.source_span(),
                                invalid: right_type.to_string(),
                                expected: "pointer".to_string()
                            }.into()); }
                        }

                        let right_value = self.visit_value(right, builder)?;
                        Ok(right_value)
                    }
                    _ => Err(InvalidOperator {
                        src: self.named_source(),
                        span: op.source_span(),
                        op: op.token_type().clone(),
                        value: "as a prefix operator in an l-value".to_string()
                    }.into())
                }
            },

            ASTNodeType::IndexExpression { left, index } => {
                let left_type = self.resolve_type(left)?;

                let elem_type = match left_type.kind {
                    CometTypeKind::Array { base_type, .. } => base_type,
                    _ => { return Err(TypeMismatch {
                        expected: "array".to_string(),
                        invalid: left_type.to_string(),
                        span: left.source_span(),
                        src: self.named_source()
                    }.into()); }
                };

                let array_value = self.visit_value(left, builder)?;
                let index = self.visit_value(index, builder)?;

                let ptr_offset = builder.ins().imul_imm_s(index, elem_type.size() as i64);
                let ptr = builder.ins().iadd(array_value, ptr_offset);

                Ok(ptr)
            },

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

    fn visit_logical_op_expr(&mut self, left: &ASTNode<'a>, op: &TokenType, right: &ASTNode<'a>, builder: &mut FunctionBuilder) -> miette::Result<ir::Value> {
        let left_type = self.resolve_type(left)?;
        let right_type = self.resolve_type(right)?;
        let unified_type = self.unify_types(&left_type, &right_type).clone();

        let end_block = builder.create_block();
        let result = builder.append_block_param(end_block, unified_type.cranelift_type);
        let short_circuit_block = builder.create_block();
        let right_block = builder.create_block();

        let left_value = self.visit_value(left, builder)?;
        let mut left_result = left_value;
        if left_type != unified_type {
            left_result = CometType::try_implicit_cast(
                left,
                left_result,
                &left_type,
                &unified_type,
                builder,
                self.named_source()
            )?;
        }

        if op == &TokenType::And {
            builder.ins().brif(left_value, right_block, &[], short_circuit_block, &[]);
            builder.seal_block(builder.current_block().unwrap());

            builder.switch_to_block(short_circuit_block);
            builder.ins().jump(end_block, &[ir::BlockArg::Value(left_result)]);
            builder.seal_block(short_circuit_block);

            builder.switch_to_block(right_block);
            let right_value = self.visit_value(right, builder)?;
            let right_result = if right_type != unified_type {
                CometType::try_implicit_cast(
                    right,
                    right_value,
                    &right_type,
                    &unified_type,
                    builder,
                    self.named_source()
                )?
            } else {
                right_value
            };
            builder.ins().jump(end_block, &[ir::BlockArg::Value(right_result)]);
            builder.seal_block(right_block);
        } else {
            builder.ins().brif(left_value, short_circuit_block, &[], right_block, &[]);
            builder.seal_block(builder.current_block().unwrap());

            builder.switch_to_block(short_circuit_block);
            builder.ins().jump(end_block, &[ir::BlockArg::Value(left_result)]);
            builder.seal_block(short_circuit_block);

            builder.switch_to_block(right_block);
            let right_value = self.visit_value(right, builder)?;
            let right_result = if right_type != unified_type {
                CometType::try_implicit_cast(
                    right,
                    right_value,
                    &right_type,
                    &unified_type,
                    builder,
                    self.named_source()
                )?
            } else {
                right_value
            };
            builder.ins().jump(end_block, &[ir::BlockArg::Value(right_result)]);
            builder.seal_block(right_block);
        }

        builder.switch_to_block(end_block);
        builder.ensure_inserted_block();

        Ok(result)
    }

    fn visit_infix_expression(&mut self, node: &ASTNode<'a>, builder: &mut FunctionBuilder) -> miette::Result<ir::Value> {
        let (left, op, right) = match node.node_type() {
            ASTNodeType::InfixExpression { left, op, right } => (left, op, right),
            _ => unreachable!()
        };
        

        if op.token_type() == &TokenType::Or || op.token_type() == &TokenType::And {
            return self.visit_logical_op_expr(left, op.token_type(), right, builder);
        }

        let mut left_side = self.visit_value(left, builder)?;
        let mut right_side = self.visit_value(right, builder)?;

        let left_type = self.resolve_type(left)?;
        let right_type = self.resolve_type(right)?;
        let unified_type = self.unify_types(&left_type, &right_type);

        let is_signed = unified_type.is_signed();
        let is_int = unified_type.is_int();

        if &left_type != unified_type {
            left_side = CometType::try_implicit_cast(
                left,
                left_side,
                &left_type,
                unified_type,
                builder,
                self.named_source()
            )?;
        }
        if &right_type != unified_type {
            right_side = CometType::try_implicit_cast(
                right,
                right_side,
                &right_type,
                unified_type,
                builder,
                self.named_source()
            )?;
        }

        let out: ir::Value;
        match op.token_type() {
            TokenType::Plus => {

                out = if is_int {
                    builder.ins().iadd(left_side, right_side)
                } else {
                    builder.ins().fadd(left_side, right_side)
                }
            },
            TokenType::Minus => {
                out = if is_int {
                    builder.ins().isub(left_side, right_side)
                } else {
                    builder.ins().fsub(left_side, right_side)
                }
            },
            TokenType::Times => {
                out = if is_int {
                    builder.ins().imul(left_side, right_side)
                } else {
                    builder.ins().fmul(left_side, right_side)
                }
            },
            TokenType::Divide => {
                out = if is_int {
                    if is_signed {
                        builder.ins().sdiv(left_side, right_side)
                    } else {
                        builder.ins().udiv(left_side, right_side)
                    }
                } else {
                    builder.ins().fdiv(left_side, right_side)
                }
            },
            TokenType::Modulo => {
                if !is_int {
                    return Err(InvalidOperator {
                        op: op.token_type().clone(),
                        span: op.source_span(),
                        src: self.named_source(),
                        value: "on non-integer".to_string()
                    }.into());
                }

                out = if is_signed {
                    builder.ins().srem(left_side, right_side)
                } else {
                    builder.ins().urem(left_side, right_side)
                }
            },
            TokenType::Ampersand => {
                if !is_int {
                    return Err(InvalidOperator {
                        op: op.token_type().clone(),
                        span: op.source_span(),
                        src: self.named_source(),
                        value: "on non-integer".to_string()
                    }.into());
                }

                out = builder.ins().band(left_side, right_side);
            }
            TokenType::Pipe => {
                if !is_int {
                    return Err(InvalidOperator {
                        op: op.token_type().clone(),
                        span: op.source_span(),
                        src: self.named_source(),
                        value: "on non-integer".to_string()
                    }.into());
                }

                out = builder.ins().bor(left_side, right_side);
            }
            TokenType::Caret => {
                if !is_int {
                    return Err(InvalidOperator {
                        op: op.token_type().clone(),
                        span: op.source_span(),
                        src: self.named_source(),
                        value: "on non-integer".to_string()
                    }.into());
                }

                out = builder.ins().bxor(left_side, right_side);
            }
            TokenType::EqEq => {
                out = if is_int {
                    builder.ins().icmp(IntCC::Equal, left_side, right_side)
                } else {
                    builder.ins().fcmp(FloatCC::Equal, left_side, right_side)
                }
            },
            TokenType::NotEq => {
                out = if is_int {
                    builder.ins().icmp(IntCC::NotEqual, left_side, right_side)
                } else {
                    builder.ins().fcmp(FloatCC::NotEqual, left_side, right_side)
                }
            },
            TokenType::Lt => {
                out = if is_int {
                    builder.ins().icmp(
                        if is_signed {IntCC::SignedLessThan} else {IntCC::UnsignedLessThan},
                        left_side,
                        right_side
                    )
                } else {
                    builder.ins().fcmp(
                        FloatCC::LessThan,
                        left_side,
                        right_side
                    )
                }
            },
            TokenType::Gt => {
                out = if is_int {
                    builder.ins().icmp(
                        if is_signed {IntCC::SignedGreaterThan} else {IntCC::UnsignedGreaterThan},
                        left_side,
                        right_side
                    )
                } else {
                    builder.ins().fcmp(
                        FloatCC::GreaterThan,
                        left_side,
                        right_side
                    )
                }
            },
            TokenType::LtEq => {
                out = if is_int {
                    builder.ins().icmp(
                        if is_signed {IntCC::SignedLessThanOrEqual} else {IntCC::UnsignedLessThanOrEqual},
                        left_side,
                        right_side
                    )
                } else {
                    builder.ins().fcmp(
                        FloatCC::LessThanOrEqual,
                        left_side,
                        right_side
                    )
                }
            },
            TokenType::GtEq => {
                out = if is_int {
                    builder.ins().icmp(
                        if is_signed {IntCC::SignedGreaterThanOrEqual} else {IntCC::UnsignedGreaterThanOrEqual},
                        left_side,
                        right_side
                    )
                } else {
                    builder.ins().fcmp(
                        FloatCC::GreaterThanOrEqual,
                        left_side,
                        right_side
                    )
                }
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

    fn visit_get_field(&mut self, node: &ASTNode<'a>, builder: &mut FunctionBuilder) -> miette::Result<ir::Value> {
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
                        invalid: struct_type.to_string(),
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

        let method = self.get_method(comet_struct.name().to_string(), field_name.to_string());
        if method.is_some() { // getting method
            let method = method.unwrap();
            let method_ref = self.module.declare_func_in_func(method.func_id, &mut builder.func);
            return Ok(builder.ins().func_addr(self.module.isa().pointer_type(), method_ref));
        }

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

    fn visit_prefix_expression(&mut self, node: &ASTNode<'a>, builder: &mut FunctionBuilder) -> miette::Result<ir::Value> {
        let (op, right_node) = match node.node_type() {
            ASTNodeType::PrefixExpression { op, right } => (op, right),
            _ => unreachable!()
        };

        let right_type = self.resolve_type(right_node)?;
        let right_value = self.visit_value(right_node, builder)?;

        match op.token_type() {
            TokenType::Ampersand => {
                let stack_slot = builder.create_sized_stack_slot(StackSlotData::new(
                    StackSlotKind::ExplicitSlot,
                    right_type.size(),
                    right_type.align() as u8
                ));

                let stack_addr = builder.ins().stack_addr(self.module.isa().pointer_type(), stack_slot, 0);
                builder.ins().store(MemFlagsData::new(), right_value, stack_addr, 0);

                Ok(stack_addr)
            },

            TokenType::Times => {
                if !right_type.is_ptr() {
                    return Err(TypeMismatch {
                        expected: "pointer".to_string(),
                        invalid: right_type.to_string(),
                        src: self.named_source(),
                        span: right_node.source_span()
                    }.into());
                }

                let internal_type = match right_type.kind {
                    CometTypeKind::Pointer(v) => v,
                    _ => unreachable!()
                };

                Ok(builder.ins().load(
                    internal_type.cranelift_type,
                    MemFlagsData::new(),
                    right_value,
                    0
                ))
            }

            TokenType::Minus => {

                let is_int = right_type.is_int();

                if is_int {
                    Ok(builder.ins().imul_imm_s(right_value, -1))
                } else {
                    let imm = builder.ins().f32const(-1.0);
                    Ok(builder.ins().fmul(right_value, imm))
                }

            }

            TokenType::Tilde => {
                let is_int = right_type.is_int();
                if !is_int {
                    return Err(InvalidOperator {
                        op: op.token_type().clone(),
                        span: op.source_span(),
                        src: self.named_source(),
                        value: "on non-integer".to_string()
                    }.into());
                }

                Ok(builder.ins().bnot(right_value))
            }

            TokenType::Not => {
                Ok(builder.ins().icmp_imm_s(IntCC::Equal, right_value, 0))
            }

            _ => Err(InvalidOperator {
                src: self.named_source(),
                span: op.source_span(),
                op: op.token_type().clone(),
                value: "as a prefix operator".to_string()
            }.into())
        }
    }

    fn visit_expression_statement(&mut self, node: &ASTNode<'a>, builder: &mut FunctionBuilder) -> miette::Result<()> {
        match node.node_type() {
            ASTNodeType::ExpressionStatement(expr) => {
                self.visit_value(expr, builder)?;
            },
            _ => unreachable!()
        }

        Ok(())
    }

    fn compile_function(
        &mut self,
        name: &str,
        args: &[ASTNode<'a>],
        return_type_node: Option<&ASTNode<'a>>,
        body: &ASTNode<'a>,
        owner: FunctionOwner
    ) -> miette::Result<FuncId> {
        let (is_struct_impl, symbol_name) = match owner {
            FunctionOwner::Global => (false, name.to_string().clone()),
            FunctionOwner::Impl(struct_name) => (true, format!("{struct_name}_{name}"))
        };

        let mut sig = self.module.make_signature();

        let mut builder_context = FunctionBuilderContext::new();
        let mut arg_types = Vec::new();
        let previous_function = self.current_function.clone();

        for (i, arg) in args.iter().enumerate() {
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
            arg_types.push(arg_type.clone());
            self.scopes.last_mut().unwrap().variables.insert(arg_name.clone(), CometVariable {
                type_: arg_type,
                var_type: CometVarType::FuncArg(i),
                mutable: false,
                function_id: None
            });
        }

        let return_type = return_type_node
            .as_ref()
            .map(|node| self.get_type_literal_type(node))
            .transpose()?
            .unwrap_or_else(CometType::new_void);

        if return_type.cranelift_type != types::INVALID {
            sig.returns.push(AbiParam::new(return_type.cranelift_type));
        }

        let new_comet_func = CometFunction {
            arg_types,
            return_type: Box::new(return_type)
        };

        self.current_function = Some(new_comet_func.clone());

        let function_type = CometType::new_function(new_comet_func);

        let func_id = self.module.declare_function(&symbol_name, Linkage::Export, &sig).unwrap();

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

        //let func_var = Variable::from_u32(self.var_index);
        //self.var_index += 1;
        let func_var = builder.declare_var(target_config.pointer_type());
        builder.def_var(func_var, func_addr);

        if !is_struct_impl {
            self.scopes.last_mut().unwrap().variables.insert(symbol_name.clone(), CometVariable {
                type_: function_type,
                var_type: Local(func_var),
                mutable: false,
                function_id: Some(func_id)
            });
        }
        
        self.scopes.push(ScopeFrame::new());

        // generate code
        let result = (|| {
            self.compile(body, Some(&mut builder))
        })();

        self.scopes.pop();
        self.current_function = previous_function;
        result?;

        builder.seal_all_blocks();

        
        // finalize func
        builder.finalize(target_config);

       

        println!("=== BUILT FUNCTION ===\n{}", ctx.func);

        self.module.define_function(func_id, &mut ctx).unwrap();
        self.module.clear_context(&mut ctx);

        Ok(func_id)
    }

    fn visit_func_def(&mut self, node: &ASTNode<'a>) -> miette::Result<()> {
        let (name_node, func_args, return_type_optional, body) = match node.node_type() {
            ASTNodeType::FuncDefinitionStatement {name, args, return_type, body } => (name, args, return_type, body),
            _ => unreachable!()
        };

        let name = match name_node.node_type() {
            ASTNodeType::IdentifierLiteral(value) => value,
            _ => unreachable!()
        };

        self.compile_function(name, func_args, return_type_optional.as_ref().map(|r| r.as_ref()), body, FunctionOwner::Global)?;

        Ok(())
        
    }

    fn visit_ret_statement(&mut self, node: &ASTNode<'a>, builder: &mut FunctionBuilder) -> miette::Result<()> {
        let ret_value_optional = match node.node_type() {
            ASTNodeType::ReturnStatement(value) => value,
            _ => unreachable!()
        };

        if ret_value_optional.is_none() {
            builder.ins().return_(&[]);
        } else {
            let ret_value_node = ret_value_optional.as_ref().unwrap();
            if matches!(self.current_function.as_ref().unwrap().return_type.kind, CometTypeKind::Void) {
                return Err(TypeMismatch {
                    expected: "void".to_string(),
                    invalid: "return value".to_string(),
                    span: ret_value_node.source_span(),
                    src: self.named_source()
                }.into());
            }

            let mut ret_value = self.visit_value(ret_value_node, builder)?;

            let ret_type = self.resolve_type(ret_value_node)?;
            if ret_type != *(self.current_function.as_ref().unwrap().return_type) {
                ret_value = CometType::try_implicit_cast(ret_value_node, ret_value, &ret_type, &*(self.current_function.as_ref().unwrap().return_type), builder, self.named_source())?;
            }

            builder.ins().return_(&[ret_value]);
        }

        Ok(())
    }

    fn visit_assign_statement(&mut self, node: &ASTNode<'a>, builder: &mut FunctionBuilder) -> miette::Result<()> {
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

            let mut value = self.visit_value(value_node, builder)?;
            let address = self.visit_l_value(ident_node, builder)?;

            if target_type != value_type {
                value = CometType::try_implicit_cast(value_node, value, &value_type, &target_type, builder, self.named_source())?;
            }

            
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
                    expected: var_type.to_string(),
                    invalid: value_type.to_string(),
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

        self.var_index += 1;

        match value_type.kind {
            CometTypeKind::Void => { return Err(InvalidVariableType {
                span: value_node.source_span(),
                src: self.named_source(),
                type_name: value_type.to_string()
            }.into()); },

            CometTypeKind::Unkown => {
                if type_node.is_none() { return Err(TypeAnnotationNeeded {
                    span: node.source_span(),
                    src: self.named_source(),
                    var_name: ident.clone()
                }.into()); }
            },
            _ => {}
        }
        
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

        let new_variable = builder.declare_var(final_type.cranelift_type);

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

        self.scopes.last_mut().unwrap().variables.insert(ident.clone(), comet_var);

        Ok(())
    }

    fn visit_match_statement(&mut self, node: &ASTNode<'a>, builder: &mut FunctionBuilder) -> miette::Result<()> {
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

    fn visit_if_statement(&mut self, node: &ASTNode<'a>, builder: &mut FunctionBuilder) -> miette::Result<()> {
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

    fn visit_while_statement(&mut self, node: &ASTNode<'a>, builder: &mut FunctionBuilder) -> miette::Result<()> {
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

    fn visit_struct_def_statement(&mut self, node: &ASTNode<'a>) -> miette::Result<Option<CometStruct>> {
        let (ident_node, field_nodes, generics) = match node.node_type() {
            ASTNodeType::StructDefinitionStatement { ident, fields, generics } => (ident, fields, generics),
            _ => unreachable!()
        };

        let ident = match ident_node.node_type() {
            ASTNodeType::IdentifierLiteral(value) => value,
            _ => unreachable!()
        };

        if generics.is_some() {
            let generic_template = node.clone();
            self.scopes.last_mut().unwrap().generics.insert(ident.clone(), generic_template);
            return Ok(None);
        }

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

        if generics.is_none() {
            self.scopes.last_mut().unwrap().types.insert(ident.clone(), CometType::new_struct(new_struct.clone()));
        }

        Ok(Some(new_struct))
    }

    fn visit_extern_directive(&mut self, name_node: &ASTNode<'a>, type_node: &ASTNode<'a>) -> miette::Result<()> {
        let name = match name_node.node_type() {
            ASTNodeType::IdentifierLiteral(value) => value,
            _ => unreachable!()
        };
        
        // make signature
        let mut sig = self.module.make_signature();
        let comet_type = self.get_type_literal_type(type_node)?;

        let func_type = match &comet_type.kind {
            CometTypeKind::Function(v) => v,
            _ => { return Err(CompilerBug {
                src: self.named_source(),
                span: type_node.source_span(),
                text: String::from("only functions are supported with #extern")
            }.into()); }
        };

        if func_type.return_type.cranelift_type != types::INVALID {
            sig.returns.push(AbiParam::new(func_type.return_type.cranelift_type));
        }
        for arg in &func_type.arg_types {
            sig.params.push(AbiParam::new(arg.cranelift_type))
        }

        // make external func with new sig
        let ext_func_id = self.module
            .declare_function(name, Linkage::Import, &sig)
            .unwrap();

        /*let data_id = self.module
            .declare_data(&format!("{}_var", name), Linkage::Local, true, false)
            .unwrap();

        let mut data_desc = DataDescription::new();
        let target_config = self.module.isa().frontend_config();
        let pointer_size = target_config.pointer_type().bytes() as usize;

        data_desc.define_zeroinit(pointer_size);

        let data_func_ref = self.module.declare_func_in_data(ext_func_id, &mut data_desc);
        data_desc.write_function_addr(0, data_func_ref);

        self.module.define_data(data_id, &data_desc).unwrap();*/

        self.scopes.last_mut().unwrap().variables.insert(name.clone(), CometVariable {
            type_: comet_type,
            var_type: CometVarType::External(ext_func_id),
            mutable: false,
            function_id: Some(ext_func_id)
        });

        Ok(())
    }

    fn visit_compiler_directive(&mut self, node: &ASTNode<'a>) -> miette::Result<()> {
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

    fn visit_impl_def_statement(&mut self, node: &ASTNode<'a>) -> miette::Result<()> {
        let (struct_node, functions) = match node.node_type() {
            ASTNodeType::ImplDefStatement { struct_type, functions } => (struct_type, functions),
            _ => unreachable!()
        };

        let is_generic_impl = match struct_node.node_type() {
            ASTNodeType::TypeLiteral { generic_types, .. } => generic_types.is_some(),
            _ => false
        };
        if is_generic_impl {
            self.generic_impls.push(node.clone());
            return Ok(());
        }

        let struct_type = self.get_type_literal_type(struct_node)?;

        self.scopes.last_mut().unwrap().types.insert("Self".to_string(), struct_type.clone());

        let comet_struct = match struct_type.kind {
            CometTypeKind::Struct(s) => s,
            _ => { return Err(TypeMismatch {
                invalid: struct_type.to_string(),
                expected: "struct".to_string(),
                span: struct_node.source_span(),
                src: self.named_source()
            }.into()) }
        };


        for function in functions {
            let (name_node, func_args, return_type_optional, body) = match function.node_type() {
                ASTNodeType::FuncDefinitionStatement {name, args, return_type, body } => (name, args, return_type, body),
                _ => unreachable!()
            };

            let name = match name_node.node_type() {
                ASTNodeType::IdentifierLiteral(value) => value,
                _ => unreachable!()
            };

            let func_owner = FunctionOwner::Impl(String::from(comet_struct.name()));
            let func_id = self.compile_function(name, func_args, return_type_optional.as_ref().map(|r| r.as_ref()), body, func_owner)?;
        
            let mut arg_types = Vec::new();
            for func_arg in func_args {
                let arg_type = match func_arg.node_type() {
                    ASTNodeType::FuncArgDefinition { name: _, type_ } => self.get_type_literal_type(type_)?,
                    _ => unreachable!()
                };

                arg_types.push(arg_type);
            }

            let return_type = return_type_optional.as_ref().map(|t| self.get_type_literal_type(&*t))
                .transpose()?
                .unwrap_or(CometType::new_void());

            let function = CometFunction {
                arg_types: arg_types,
                return_type: Box::new(return_type)
            };

            self.methods.insert((comet_struct.name().to_string(), name.clone()), CometMethod {
                function: function,
                func_id: func_id
            });
        }

        Ok(())
    }
    // END OF VISIT METHODS //

    pub fn compile(&mut self, ast: &ASTNode<'a>, builder: Option<&mut FunctionBuilder>) -> miette::Result<()> {
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
            ASTNodeType::StructDefinitionStatement { ident: _, fields: _, generics: _ } => {
                self.visit_struct_def_statement(ast)?;
                return Ok(());
            },
            ASTNodeType::ImplDefStatement { struct_type: _, functions: _ } => { return self.visit_impl_def_statement(ast); },

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