use cranelift_codegen::ir::{Type, Value, types};
use cranelift_frontend::FunctionBuilder;
use cranelift::prelude::InstBuilder;
use cranelift_native::builder;

use crate::comet_struct::CometStruct;

#[derive(Clone, Eq, Debug)]
pub struct CometType {
    pub cranelift_type: types::Type,
    pub signed: bool,
    pub comet_struct: Option<CometStruct>
}

impl CometType {
    pub fn new(cranelift_type: types::Type) -> Self {
        CometType { 
            cranelift_type: cranelift_type,
            signed: false,
            comet_struct: None
        }
    }
    pub fn new_int(cranelift_type: types::Type, signed: bool) -> Self {
        CometType { 
            cranelift_type: cranelift_type,
            signed: signed,
            comet_struct: None
        }
    }

    pub fn new_struct(comet_struct: CometStruct) -> Self {
        CometType {
            cranelift_type: types::I64,
            signed: false,
            comet_struct: Some(comet_struct)
        }
    }

    pub fn try_implicit_cast(value: Value, value_type: &CometType, target_type: &CometType, builder: &mut FunctionBuilder) -> Result<Value, String> {
        let their_type = target_type.cranelift_type;

        if value_type == target_type {
            return Ok(value);
        }

        let value_type = value_type.cranelift_type;

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

        if ((parent_type.is_int() && child_type.is_int()) && parent_type.wider_or_equal(child_type)) && self.signed == other.signed {
            return true;
        }

        if parent_type.is_float() && child_type.is_float() {
            return true;
        }

        false
    }
}