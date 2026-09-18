use cranelift_codegen::ir::{Type, Value, types};
use cranelift_frontend::FunctionBuilder;
use cranelift::prelude::InstBuilder;
use cranelift_native::builder;

#[derive(Clone, Eq)]
pub struct CometType {
    pub cranelift_type: types::Type
}

impl CometType {
    pub fn new(cranelift_type: types::Type) -> Self {
        CometType { 
            cranelift_type: cranelift_type
        }
    }

    pub fn try_implicit_cast(value: Value, target_type: &CometType, builder: &mut FunctionBuilder) -> Result<Value, String> {
        let value_type = builder.func.dfg.value_type(value);
        let their_type = target_type.cranelift_type;

        let out: Value;
        if value_type.is_int() && their_type.is_int() {
            if value_type.bits() < their_type.bits() { // cast from smaller to bigger int
                out = builder.ins().sextend(target_type.cranelift_type, value);
            } else if value_type.bits() > their_type.bits() { // cast from bigger to smaller int
                out = builder.ins().ireduce(their_type, value);
            } else {
                return Err(format!("Cannot convert int type '{}' to type '{}'", value_type, their_type));
            }
        } else if value_type.is_int() && their_type.is_float() { // cast from int to float
            out = builder.ins().fcvt_from_sint(their_type, value);
        } else if value_type.is_float() && their_type.is_int() { // cast from float to int
            out = builder.ins().fcvt_to_sint(their_type, value);
        } else {
            return Err(format!("Cannot implicitly cast type '{}' to type '{}'", value_type, their_type));
        }

        Ok(out)
    }
}

impl PartialEq for CometType {
    fn eq(&self, other: &Self) -> bool {

        let parent_type = self.cranelift_type;
        let child_type = other.cranelift_type;

        if (parent_type.is_int() && child_type.is_int()) && parent_type.wider_or_equal(child_type) {
            return true;
        }

        if parent_type.is_float() && child_type.is_float() {
            return true;
        }

        false
    }
}