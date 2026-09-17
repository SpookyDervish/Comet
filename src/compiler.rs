use cranelift_codegen::isa::TargetFrontendConfig;
use cranelift_codegen::{ir::AbiParam, settings};
use cranelift_codegen::ir::{self, InstBuilder, types};
use cranelift_frontend::{FunctionBuilder, FunctionBuilderContext};
use cranelift_module::{Module, default_libcall_names};
use cranelift_object::{ObjectBuilder, ObjectModule};

use crate::{ast::{ASTNode, ASTNodeType}, comet_type::CometType, scope::Scope, token::TokenType};

pub struct Compiler <'a> {
    module: ObjectModule,
    type_map: Scope<'a, &'a str, CometType>
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

        type_map.insert("i64", CometType::new(types::I64));
        type_map.insert("i32", CometType::new(types::I32));
        type_map.insert("i16", CometType::new(types::I16));
        type_map.insert("i8", CometType::new(types::I8));

        Ok(Compiler {
            module: module,
            type_map: type_map
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
        let identNode = match node.node_type() {
            ASTNodeType::TypeLiteral(value) => &**value,
            _ => unreachable!()
        };

        let ident = match identNode.node_type() {
            ASTNodeType::IdentifierLiteral(value) => value,
            _ => unreachable!()
        };

        let comet_type = self.type_map.get(&ident.as_str());
        comet_type.ok_or(format!("No"))
    }
    // END OF UTIL METHODS

    // VISIT METHODS //
    fn visit_program(&mut self, node: &ASTNode) -> Result<(), String> {
        let nodes = match node.node_type() {
            ASTNodeType::Program(value) => value,
            _ => unreachable!()
        };

        for node in nodes {
            self.compile(node)?;
        }

        Ok(())
    }

    fn visit_block(&mut self, node: &ASTNode) -> Result<(), String> {
        let nodes = match node.node_type() {
            ASTNodeType::Block(value) => value,
            _ => unreachable!()
        };

        for node in nodes {
            self.compile(node)?;
        }

        Ok(())
    }

    fn visit_value(&mut self, node: &ASTNode, builder: &mut FunctionBuilder) -> Result<ir::Value, String> {
        match node.node_type() {
            ASTNodeType::InfixExpression { left, op, right } => self.visit_infix_expression(node, builder),
            _ => Err(format!("bru"))
        }
    }

    fn visit_infix_expression(&mut self, node: &ASTNode, builder: &mut FunctionBuilder) -> Result<ir::Value, String> {
        let (left, op, right) = match node.node_type() {
            ASTNodeType::InfixExpression { left, op, right } => (left, op, right),
            _ => unreachable!()
        };

        let left_side = self.visit_value(left)?;
        let right_side = self.visit_value(left)?;

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

    fn visit_expression_statement(&mut self, node: &ASTNode) -> Result<(), String> {
        match node.node_type() {
            ASTNodeType::ExpressionStatement(expr) => {
                self.visit_value(expr)?;
            },
            _ => unreachable!()
        }

        Ok(())
    }

    fn visit_func_def(&mut self, node: &ASTNode) -> Result<(), String> {
        let mut ctx = self.module.make_context();

        let mut sig = self.module.make_signature();
        ctx.func.signature = sig;

        let mut builder_context = FunctionBuilderContext::new();

        let (name, funcArgs, return_type_optional, body) = match node.node_type() {
            ASTNodeType::FuncDefinitionStatement {name, args, return_type, body } => (name, args, return_type, body),
            _ => unreachable!()
        };

        for arg in funcArgs {
            let (arg_name, arg_type)= match arg.node_type() {
                ASTNodeType::FuncArgDefinition { name, type_ } => (name, type_),
                _ => unreachable!()
            };

            ctx.func.signature.params.push(AbiParam::new(self.get_type_literal_type(arg_type)?.cranelift_type));
        }

        if return_type_optional.is_some() {
            let return_type_node = return_type_optional.as_ref().unwrap();
            let return_type = self.get_type_literal_type(&return_type_node)?;

            ctx.func.signature.returns.push(AbiParam::new(return_type.cranelift_type));
        }

        let mut builder = FunctionBuilder::new(&mut ctx.func, &mut builder_context);

        let entry_block = builder.create_block();
        builder.append_block_params_for_function_params(entry_block);
        builder.switch_to_block(entry_block);

        // generate code
        self.compile(body, Some(&mut builder))?;

        builder.seal_block(entry_block);
        builder.ins().return_(&[]);
        
        let target_config = self.module.isa().frontend_config();
        builder.finalize(target_config);

        //println!("{}", ctx.func.display());
        

        Ok(())
    }
    // END OF VISIT METHODS //

    pub fn compile(&mut self, ast: &ASTNode, builder: Option<&mut FunctionBuilder>) -> Result<(), String> {
        match ast.node_type() {
            ASTNodeType::Program(_) => { return self.visit_program(ast); },
            ASTNodeType::Block(_) => { return self.visit_block(ast); },
            ASTNodeType::FuncDefinitionStatement {name: _, args: _, return_type: _, body: _ } => { return self.visit_func_def(ast); },
            ASTNodeType::ExpressionStatement(_) => { return self.visit_expression_statement(ast); },

            _ => {
                Err(format!("No compiler visit method for {:?}", ast.node_type()))
            }
        }
    }
}