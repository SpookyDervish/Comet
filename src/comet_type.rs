use cranelift_codegen::ir::types;

pub struct CometType {
    pub cranelift_type: types::Type
}

impl CometType {
    pub fn new(cranelift_type: types::Type) -> Self {
        CometType { 
            cranelift_type: cranelift_type
        }
    }
}