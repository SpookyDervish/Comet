use cranelift_codegen::settings;
use cranelift_module::default_libcall_names;
use cranelift_object::{ObjectBuilder, ObjectModule};

use crate::ast::{ASTNode, ASTNodeType};

pub struct Compiler {
    module: ObjectModule
}

impl Compiler {
    pub fn new() -> Result<Self, Box<dyn std::error::Error>> {
        let isa_builder = cranelift_native::builder()?;
        let isa = isa_builder.finish(settings::Flags::new(settings::builder()))?;

        let obj_builder = ObjectBuilder::new(
            isa,
            "comet_module",
            default_libcall_names()
        )?;

        let module = ObjectModule::new(obj_builder);

        Ok(Compiler {
            module: module
        })
    }

    pub fn end_module(self) -> Result<Vec<u8>, cranelift_object::object::write::Error> {
        let module = self.module;

        let product = module.finish();
        let bytes = product.emit();

        return bytes;
    }

    // VISIT METHODS //
    fn visit_program(&self, node: &ASTNode) -> Result<(), String> {
        let nodes: &Vec<ASTNode>;
        match node.node_type() {
            ASTNodeType::Program(value) => { nodes = value; },
            _ => unreachable!()
        }

        for node in nodes {
            self.compile(node)?;
        }

        Ok(())
    }
    // END OF VISIT METHODS //

    pub fn compile(&self, ast: &ASTNode) -> Result<(), String> {
        match ast.node_type() {
            ASTNodeType::Program(_) => { return self.visit_program(ast); },

            _ => {
                Err(format!("No compiler visit method for {:?}", ast.node_type()))
            }
        }
    }
}