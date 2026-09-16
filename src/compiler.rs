use cranelift_codegen::settings;
use cranelift_jit::JITBuilder;
use cranelift_module::default_libcall_names;
use cranelift_object::{ObjectBuilder, ObjectModule};
use target_lexicon::Triple;

use crate::ast::ASTNode;

pub struct Compiler {
    ast: ASTNode,

    module: ObjectModule
}

impl Compiler {
    pub fn new(ast: ASTNode) -> Result<Self, Box<dyn std::error::Error>> {
        let triple = Triple::host();

        let isa_builder = cranelift_native::builder()?;
        let isa = isa_builder.finish(settings::Flags::new(settings::builder()))?;

        let obj_builder = ObjectBuilder::new(
            isa,
            "comet_module",
            default_libcall_names()
        )?;

        let module = ObjectModule::new(obj_builder);

        Ok(Compiler {
            ast: ast,

            module: module
        })
    }

    fn end_module(self) -> Result<Vec<u8>, cranelift_object::object::write::Error> {
        let module = self.module;

        let product = module.finish();
        let bytes = product.emit();

        return bytes;
    }

    pub fn compile(self) -> Result<Vec<u8>, cranelift_object::object::write::Error> {
        

        self.end_module()
    }
}