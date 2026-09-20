use cranelift_codegen::ir::condcodes::IntCC;
use cranelift_codegen::isa::CallConv;
use cranelift_codegen::{ir::AbiParam, settings};
use cranelift_codegen::ir::{self, Block, InstBuilder, Signature, Value, types};
use cranelift_frontend::{FunctionBuilder, FunctionBuilderContext, Variable};
use cranelift_module::{FuncId, Linkage, Module, default_libcall_names};
use cranelift_object::{ObjectBuilder, ObjectModule};
use std::cell::RefCell;

use crate::scope::CometVarType::Local;
use crate::scope::{CometVarType, CometVariable, ScopeFrame};
use crate::{ast::{ASTNode, ASTNodeType}, comet_type::CometType, token::TokenType};

pub struct Compiler <'a> {
    module: ObjectModule,
    scopes: Vec<ScopeFrame<'a>>,

    var_index: u32
}

impl <'a> Compiler <'a> {
    pub fn new() -> Result<Self, Box<dyn std::error::Error>> {
        let isa_builder = cranelift_native::builder()?;
        let isa = isa_builder.finish(settings::Flags::new(settings::builder()))?;

        let obj_builder = ObjectBuilder::new(
            isa,
            "comet_module",
            default_libcall_names()
        )?;

        let module = ObjectModule::new(obj_builder);

        let mut base_frame = ScopeFrame::new();

        base_frame.types.insert("i8", CometType::new(types::I8));
        base_frame.types.insert("i16", CometType::new(types::I16));
        base_frame.types.insert("i32", CometType::new(types::I32));
        base_frame.types.insert("i64", CometType::new(types::I64));
        base_frame.types.insert("f16", CometType::new(types::F16));
        base_frame.types.insert("f32", CometType::new(types::F32));
        base_frame.types.insert("f64", CometType::new(types::F64));
        

        Ok(Compiler {
            module: module,
            scopes: vec!{base_frame},

            var_index: 0
        })
    }

    pub fn end_module(self) -> Result<Vec<u8>, cranelift_object::object::write::Error> {
        let module = self.module;

        let product = module.finish();
        let bytes = product.emit();

        return bytes;
    }

    // UTIL METHODS //
    fn get_variable(&self, name: &str) -> Option<&CometVariable> {
        self.scopes.iter().rev().find_map(|scope| scope.variables.get(name))
    }

    fn get_type(&self, name: &str) -> Option<&CometType> {
        self.scopes.iter().rev().find_map(|scope| scope.types.get(name))
    }

    fn get_type_literal_type <'b> (&'b self, node: &'b ASTNode) -> Result<&'b CometType, String> {
        let ident_node = match node.node_type() {
            ASTNodeType::TypeLiteral(value) => &**value,
            _ => unreachable!()
        };

        let ident = match ident_node.node_type() {
            ASTNodeType::IdentifierLiteral(value) => value,
            _ => unreachable!()
        };

        let comet_type = self.get_type(&ident.as_str());
        comet_type.ok_or(format!("Unkown type '{}'", ident))
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

    fn resolve_type(&self, node: &ASTNode) -> Result<CometType, String> {
        match node.node_type() {
            ASTNodeType::IntLiteral(_) => {
                Ok(CometType {
                    cranelift_type: types::I64
                })
            },

            ASTNodeType::FloatLiteral(_) => {
                Ok(CometType {
                    cranelift_type: types::F64
                })
            },

            ASTNodeType::IdentifierLiteral(name) => {
                let var = self.get_variable(&name.as_str()).ok_or(format!("Use of undefined variable '{}'", name)).unwrap();

                Ok(var.type_.clone())
            },

            ASTNodeType::InfixExpression { left, op, right } => {
                let left_value = self.resolve_type(left)?;

                match op.token_type() {
                    TokenType::Divide => { return Ok(CometType { cranelift_type: types::F64 }); },

                    _ => {}
                }

                let right_value = self.resolve_type(right)?;
                return Ok(self.unify_types(&left_value, &right_value).clone());
            },

            ASTNodeType::FuncCall { left, args } => {
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
                let out_type = CometType { cranelift_type: types::I64 };

                Ok(out_type)
            }

            _ => Err(format!("Can't resolve type of '{:?}'", node.node_type()))
        }
    }
    // END OF UTIL METHODS

    // VISIT METHODS //
    fn visit_program(&mut self, node: &'a ASTNode) -> Result<(), String> {
        let nodes = match node.node_type() {
            ASTNodeType::Program(value) => value,
            _ => unreachable!()
        };

        for node in nodes {
            self.compile(node, None)?;
        }

        Ok(())
    }

    fn visit_block(&mut self, node: &'a ASTNode, builder: &mut FunctionBuilder) -> Result<(), String> {
        let nodes = match node.node_type() {
            ASTNodeType::Block(value) => value,
            _ => unreachable!()
        };

        for node in nodes {
            self.compile(node, Some(builder))?;
        }

        Ok(())
    }

    fn visit_value(&mut self, node: &ASTNode, builder: &mut FunctionBuilder) -> Result<ir::Value, String> {
        match node.node_type() {
            ASTNodeType::InfixExpression { left: _, op: _, right: _ } => self.visit_infix_expression(node, builder),

            ASTNodeType::IntLiteral(num) => {
                Ok(builder.ins().iconst(types::I64, *num as i64))
            },

            ASTNodeType::FloatLiteral(num) => {
                Ok(builder.ins().f64const(*num as f64))
            },

            ASTNodeType::IdentifierLiteral(var_name) => {
                let comet_var = self.get_variable(&var_name.as_str()).ok_or(format!("Use of undefined variable '{}'", var_name)).unwrap();

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
                        .ok_or(format!("'{}' is not a callable function", name))?,
                    _ => return Err("Function calls require a function identifier".to_string())
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

            _ => Err(format!("Cannot compile r-value '{:?}'", node.node_type()))
        }
    }

    fn visit_infix_expression(&mut self, node: &ASTNode, builder: &mut FunctionBuilder) -> Result<ir::Value, String> {
        let (left, op, right) = match node.node_type() {
            ASTNodeType::InfixExpression { left, op, right } => (left, op, right),
            _ => unreachable!()
        };

        let left_side = self.visit_value(left, builder)?;
        let right_side = self.visit_value(right, builder)?;

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

            _ => {
                return Err(format!("Invalid operator for infix expression '{:?}'", op.token_type()));
            }
        }

        Ok(out)
    }

    fn visit_expression_statement(&mut self, node: &ASTNode, builder: &mut FunctionBuilder) -> Result<(), String> {
        match node.node_type() {
            ASTNodeType::ExpressionStatement(expr) => {
                self.visit_value(expr, builder)?;
            },
            _ => unreachable!()
        }

        Ok(())
    }

    fn visit_func_def(&mut self, node: &'a ASTNode) -> Result<(), String> {

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
            self.scopes.last_mut().unwrap().variables.insert(arg_name, CometVariable {
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

        let func_id = self.module.declare_function(name, Linkage::Export, &sig).unwrap();

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

        self.scopes.last_mut().unwrap().variables.insert(name, CometVariable {
            type_: CometType { cranelift_type: target_config.pointer_type() },
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

    fn visit_ret_statement(&mut self, node: &ASTNode, builder: &mut FunctionBuilder) -> Result<(), String> {
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

    fn visit_assign_statement(&mut self, node: &'a ASTNode, builder: &mut FunctionBuilder) -> Result<(), String> {
        let (ident_node, type_node, value_node) = match node.node_type() {
            ASTNodeType::AssignStatement { ident, type_, value } => (ident, type_, value),
            _ => unreachable!()
        };

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
                return Err(format!("Cannot reassign immutable variable"));
            }

            let var_type = &existing_var.unwrap().type_;
            if value_type != *var_type {
                return Err(format!("Attempted to change type of variable '{}' at runtime", ident));
            }

            if type_node.is_some() {
                return Err(format!("Cannot type annotate reassignment"));
            }

            match existing_var.unwrap().var_type {
                CometVarType::Local(var) => { builder.def_var(var, value); },
                _ => unreachable!()
            }

            return Ok(());
        }

        let new_variable = Variable::from_u32(self.var_index);
        self.var_index += 1;
        
        let mut final_type = &value_type;
        if type_node.is_some() {
            // get type of type annotation
            let var_type = self.get_type_literal_type(type_node.as_ref().unwrap())?;

            if var_type != &value_type {
                value = CometType::try_implicit_cast(value, &var_type, builder)?;
                final_type = var_type;
            } else {
                final_type = self.unify_types(&final_type, &var_type);
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
            type_: final_type.clone(),
            var_type: CometVarType::Local(new_variable),
            mutable: true,
            function_id
        };

        self.scopes.last_mut().unwrap().variables.insert(ident.as_str(), comet_var);

        Ok(())
    }

    fn visit_match_statement(&mut self, node: &'a ASTNode, builder: &mut FunctionBuilder) -> Result<(), String> {
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

    fn visit_if_statement(&mut self, node: &'a ASTNode, builder: &mut FunctionBuilder) -> Result<(), String> {
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
    // END OF VISIT METHODS //

    pub fn compile(&mut self, ast: &'a ASTNode, builder: Option<&mut FunctionBuilder>) -> Result<(), String> {
        match ast.node_type() {
            ASTNodeType::Program(_) => { return self.visit_program(ast); },
            ASTNodeType::Block(_) => { return self.visit_block(ast, builder.unwrap()); },

            ASTNodeType::FuncDefinitionStatement {name: _, args: _, return_type: _, body: _ } => { return self.visit_func_def(ast); },
            ASTNodeType::ExpressionStatement(_) => { return self.visit_expression_statement(ast, builder.unwrap()); },
            ASTNodeType::ReturnStatement(_) => { return self.visit_ret_statement(ast, builder.unwrap()); }
            ASTNodeType::AssignStatement { ident: _, type_: _, value: _ } => { return self.visit_assign_statement(ast, builder.unwrap()); }
            ASTNodeType::MatchStatement { expr: _, nodes: _, default: _ } => { return self.visit_match_statement(ast, builder.unwrap()); },
            ASTNodeType::IfStatement { expr: _, body: _, else_body: _ } => { return self.visit_if_statement(ast, builder.unwrap()) },

            _ => {
                Err(format!("No compiler visit method for {:?}", ast.node_type()))
            }
        }
    }
}