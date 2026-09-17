use cranelift_codegen::{ir::AbiParam, settings};
use cranelift_codegen::ir::{self, Function, InstBuilder, UserFuncName, types};
use cranelift_frontend::{FunctionBuilder, FunctionBuilderContext, Variable};
use cranelift_module::{Linkage, Module, default_libcall_names};
use cranelift_object::{ObjectBuilder, ObjectModule};

use crate::{ast::{ASTNode, ASTNodeType}, comet_type::CometType, scope::Scope, token::TokenType};

pub struct CometVariable {
    type_: CometType,
    var: Variable
}

pub struct Compiler <'a> {
    module: ObjectModule,
    type_map: Scope<'a, &'a str, CometType>,

    var_map: Scope<'a, &'a str, CometVariable>,

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

        let mut type_map = Scope::new(None);
        type_map.insert("i8", CometType::new(types::I8));
        type_map.insert("i16", CometType::new(types::I16));
        type_map.insert("i32", CometType::new(types::I32));
        type_map.insert("i64", CometType::new(types::I64));
        type_map.insert("f16", CometType::new(types::F16));
        type_map.insert("f32", CometType::new(types::F32));
        type_map.insert("f64", CometType::new(types::F64));
        

        Ok(Compiler {
            module: module,
            type_map: type_map,

            var_map: Scope::new(None),

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
    fn get_type_literal_type <'b> (&'b self, node: &'b ASTNode) -> Result<&'b CometType, String> {
        let ident_node = match node.node_type() {
            ASTNodeType::TypeLiteral(value) => &**value,
            _ => unreachable!()
        };

        let ident = match ident_node.node_type() {
            ASTNodeType::IdentifierLiteral(value) => value,
            _ => unreachable!()
        };

        let comet_type = self.type_map.get(&ident.as_str());
        comet_type.ok_or(format!("Unkown type '{}'", ident))
    }

    fn resolve_type(&self, node: &ASTNode) -> Result<CometType, String> {
        match node.node_type() {
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
            ASTNodeType::InfixExpression { left, op, right } => self.visit_infix_expression(node, builder),

            ASTNodeType::IntLiteral(num) => {
                Ok(builder.ins().iconst(types::I64, *num as i64))
            },

            ASTNodeType::FloatLiteral(num) => {
                Ok(builder.ins().f64const(*num as f64))
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
            }

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

        let (name_node, funcArgs, return_type_optional, body) = match node.node_type() {
            ASTNodeType::FuncDefinitionStatement {name, args, return_type, body } => (name, args, return_type, body),
            _ => unreachable!()
        };

        let name = match name_node.node_type() {
            ASTNodeType::IdentifierLiteral(value) => value,
            _ => unreachable!()
        };

        let func_id = self.module.declare_function(name, Linkage::Export, &sig).unwrap();

        let mut builder_context = FunctionBuilderContext::new();

        for arg in funcArgs {
            let (arg_name, arg_type)= match arg.node_type() {
                ASTNodeType::FuncArgDefinition { name, type_ } => (name, type_),
                _ => unreachable!()
            };

            sig.params.push(AbiParam::new(self.get_type_literal_type(arg_type)?.cranelift_type));
        }

        if return_type_optional.is_some() {
            let return_type_node = return_type_optional.as_ref().unwrap();
            let return_type = self.get_type_literal_type(&return_type_node)?;

            sig.returns.push(AbiParam::new(return_type.cranelift_type));
            
        }

        let mut ctx = self.module.make_context();
        ctx.func.signature = sig;

        let mut builder = FunctionBuilder::new(&mut ctx.func, &mut builder_context);
        

        let entry_block = builder.create_block();
        builder.append_block_params_for_function_params(entry_block);
        builder.switch_to_block(entry_block);

        // generate code
        self.compile(body, Some(&mut builder))?;

        builder.seal_block(entry_block);
        
        let target_config = self.module.isa().frontend_config();
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

    fn visit_let_statement(&mut self, node: &'a ASTNode, builder: &mut FunctionBuilder) -> Result<(), String> {
        let (ident_node, type_node, value_node) = match node.node_type() {
            ASTNodeType::LetStatement { ident, type_, value } => (ident, type_, value),
            _ => unreachable!()
        };

        let ident = match ident_node.node_type() {
            ASTNodeType::IdentifierLiteral(value) => value,
            _ => unreachable!()
        };

        let existing_var = self.var_map.get(&ident.as_str());
        if existing_var.is_some() {
            return Err(format!("Redeclaration of variable '{}'", ident));
        }

        let value = self.visit_value(value_node, builder)?;
        
        let new_variable = Variable::from_u32(self.var_index);
        self.var_index += 1;

        builder.def_var(new_variable, value);

        let var_type = self.resolve_type(value_node)?;

        let comet_var = CometVariable {
            type_: var_type,
            var: new_variable
        };

        self.var_map.insert(ident.as_str(), comet_var);

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
            ASTNodeType::LetStatement { ident: _, type_: _, value: _ } => { return self.visit_let_statement(ast, builder.unwrap()); }

            _ => {
                Err(format!("No compiler visit method for {:?}", ast.node_type()))
            }
        }
    }
}