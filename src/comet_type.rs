use cranelift_codegen::ir::{Value, types};
use cranelift_frontend::FunctionBuilder;
use cranelift::prelude::InstBuilder;
use cranelift_module::FuncId;
use miette::NamedSource;

use itertools::Itertools;
use std::fmt;

use crate::{ast::ASTNode, comet_error::{InvalidCast, TypeMismatch}, comet_struct::CometStruct};

#[derive(Debug)]
pub struct CometMethod {
    pub function: CometFunction,
    pub func_id: FuncId
}

pub enum FunctionOwner {
    Global,
    Impl(String)
}

#[derive(Clone, Eq, PartialEq, Debug)]
pub struct CometFunction {
    pub arg_types: Vec<CometType>,
    pub return_type: Box<CometType>


}

#[derive(Clone, Eq, PartialEq, Debug)]
pub enum CometTypeKind {
    Struct(CometStruct),
    Function(CometFunction),
    Scalar(bool),
    Pointer(Box<CometType>),
    Array { base_type: Box<CometType>, size: u32 },
    Generic(String),
    Void,
    Unkown
}

#[derive(Clone, Eq, Debug)]
pub struct CometType {
    pub cranelift_type: types::Type,
    pub kind: CometTypeKind
}

impl CometType {
    pub fn new(cranelift_type: types::Type) -> Self {
        CometType { 
            cranelift_type: cranelift_type,
            kind: CometTypeKind::Scalar(false)
        }
    }
    pub fn new_ptr(base_type: CometType, pointer_type: types::Type) -> Self {
        CometType {
            cranelift_type: pointer_type,
            kind: CometTypeKind::Pointer(Box::new(base_type))
        }
    }
    pub fn new_array(base_type: CometType, size: u32, pointer_type: types::Type) -> Self {
        CometType {
            cranelift_type: pointer_type,
            kind: CometTypeKind::Array { base_type: Box::new(base_type), size: size }
        }
    }
    pub fn new_int(cranelift_type: types::Type, signed: bool) -> Self {
        CometType { 
            cranelift_type: cranelift_type,
            kind: CometTypeKind::Scalar(signed)
        }
    }
    pub fn new_struct(comet_struct: CometStruct) -> Self {
        CometType {
            cranelift_type: types::I64,
            kind: CometTypeKind::Struct(comet_struct)
        }
    }
    pub fn new_function(comet_function: CometFunction) -> Self {
        CometType {
            cranelift_type: types::I64,
            kind: CometTypeKind::Function(comet_function)
        }
    }
    pub fn new_generic(name: String) -> Self {
        CometType {
            cranelift_type: types::INVALID,
            kind: CometTypeKind::Generic(name)
        }
    }
    pub fn new_void() -> Self {
        CometType { cranelift_type: types::INVALID, kind: CometTypeKind::Void }
    }
    pub fn new_unkown() -> Self {
        CometType { cranelift_type: types::INVALID, kind: CometTypeKind::Unkown }
    }

    pub fn is_signed(&self) -> bool {
        match self.kind {
            CometTypeKind::Scalar(s) => s,
            _ => false
        }
    }
    pub fn is_int(&self) -> bool {
        match self.kind {
            CometTypeKind::Scalar(_) => self.cranelift_type.is_int(),
            _ => false
        }
    }
    pub fn is_float(&self) -> bool {
        match self.kind {
            CometTypeKind::Scalar(_) => self.cranelift_type.is_float(),
            _ => false
        }
    }
    pub fn is_void(&self) -> bool {
        match self.kind {
            CometTypeKind::Void => true,
            _ => false
        }
    }
    pub fn is_ptr(&self) -> bool {
        match self.kind {
            CometTypeKind::Pointer(_) => true,
            _ => false
        }
    }
    pub fn is_array(&self) -> bool {
        match self.kind {
            CometTypeKind::Array { .. } => true,
            _ => false
        }
    }

    pub fn size(&self) -> u32 {
        match &self.kind {
            CometTypeKind::Function(_) |
            CometTypeKind::Scalar(_) |
            CometTypeKind::Pointer(_) => self.cranelift_type.bytes(),
            CometTypeKind::Struct(s) => s.get_layout().1,
            CometTypeKind::Array{ base_type, size } => base_type.size() * size,
            CometTypeKind::Generic(_) => 0,
            CometTypeKind::Void => 0,
            CometTypeKind::Unkown => 0
        }
    }
    pub fn align(&self) -> u32 {
        match &self.kind {
            CometTypeKind::Function(_) |
            CometTypeKind::Scalar(_) |
            CometTypeKind::Pointer(_) => self.cranelift_type.bytes(),
            CometTypeKind::Struct(s) => s.get_layout().2,
            CometTypeKind::Array { base_type, .. } => base_type.align(),
            CometTypeKind::Generic(_) => 0,
            CometTypeKind::Void => 0,
            CometTypeKind::Unkown => 0
        }
    }

    pub fn try_implicit_cast(value_node: &ASTNode, value: Value, value_comet_type: &CometType, target_type: &CometType, builder: &mut FunctionBuilder, named_source: NamedSource<String>) -> miette::Result<Value> {
        let their_type = target_type.cranelift_type;

        if value_comet_type == target_type {
            return Ok(value);
        }

        let value_type = value_comet_type.cranelift_type;

        let out: Value;

        // cast one int type to another
        if value_comet_type.is_int() && target_type.is_int() {
            let target_signed = match target_type.kind {
                CometTypeKind::Scalar(signed) => signed,
                _ => unreachable!()
            };

            let value_signed = match value_comet_type.kind {
                CometTypeKind::Scalar(signed) => signed,
                _ => unreachable!()
            };

            if value_type.bits() < their_type.bits() { // cast from smaller to bigger int

                if value_signed {
                    out = builder.ins().sextend(target_type.cranelift_type, value);
                } else {
                    out = builder.ins().uextend(target_type.cranelift_type, value);
                }
            } else if value_type.bits() > their_type.bits() { // cast from bigger to smaller int
                out = builder.ins().ireduce(their_type, value);
            } else if target_signed != value_signed {
                // do nothing, because ints in cranelift dont care about signedness
                out = value;
            } else {
                return Err(InvalidCast { // dont implicitly cast from bigger type to smaller type
                    new_type: target_type.cranelift_type.to_string(),
                    old_type: value_type.to_string(),
                    span: value_node.source_span(),
                    src: named_source
                }.into());
            }
        } else if value_type.is_int() && their_type.is_float() { // cast from int to float
            let value_signed = match value_comet_type.kind {
                CometTypeKind::Scalar(signed) => signed,
                _ => unreachable!()
            };

            out = if value_signed {
                builder.ins().fcvt_from_sint(their_type, value)
            } else {
                builder.ins().fcvt_from_uint(their_type, value)
            }
        } else if value_type.is_float() && their_type.is_int() { // cast from float to int
            let target_signed = match target_type.kind {
                CometTypeKind::Scalar(signed) => signed,
                _ => unreachable!()
            };

            out = if target_signed {
                builder.ins().fcvt_to_sint(their_type, value)
            } else {
                builder.ins().fcvt_to_uint(their_type, value)
            }
        } else if value_type.is_float() && their_type.is_float() {
            if value_type.bits() < their_type.bits() {
                out = builder.ins().fpromote(their_type, value);
            } else { // dont implicitly cast from bigger type to smaller type
                return Err(InvalidCast { 
                    new_type: target_type.to_string(),
                    old_type: value_comet_type.to_string(),
                    span: value_node.source_span(),
                    src: named_source
                }.into());
            }
        } else {
            return Err(TypeMismatch {
                expected: target_type.to_string(),
                invalid: value_comet_type.to_string(),
                src: named_source,
                span: value_node.source_span()
            }.into());
        }

        Ok(out)
    }

    pub fn generic_type_name(&self) -> String {
        match &self.kind {
            CometTypeKind::Function(f) => format!("f_{}_ret_{}", f.arg_types.iter().format("_").to_string(), f.return_type),
            CometTypeKind::Generic(n) => format!("g_{}", n),
            CometTypeKind::Pointer(t) => format!("p_{}", t),
            CometTypeKind::Scalar(s) => format!("{}{}", self.cranelift_type, if *s {"_s"} else {""}),
            CometTypeKind::Struct(s) => format!("st_{}", s.name()),
            CometTypeKind::Array { base_type, .. } => format!("a_{}", base_type.generic_type_name()),
            CometTypeKind::Void => format!("v"),
            CometTypeKind::Unkown => unreachable!()
        }
    }
}

impl fmt::Display for CometType {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match &self.kind {
            CometTypeKind::Scalar(v) => write!(f, "{}{}", if *v {"signed-"} else {""}, self.cranelift_type),
            CometTypeKind::Function(func) => {
                write!(f, "fun(")?;
                for (i, arg_type) in func.arg_types.iter().enumerate() {
                    if i > 0 {
                        write!(f, ", ")?;
                    }
                    write!(f, "{}", arg_type.to_string())?;
                }
                write!(f, ")")?;

                if !func.return_type.is_void() {
                    write!(f, " :: {}", func.return_type.to_string())?;
                }

                Ok(())
            },
            CometTypeKind::Pointer(t) => write!(f, "&{}", (*t).to_string()),
            CometTypeKind::Struct(s) => write!(f, "{}{{}}", s.name()),
            CometTypeKind::Generic(name) => write!(f, "{}", name),
            CometTypeKind::Array { base_type, size } => write!(f, "{}[{}]", base_type, size),
            CometTypeKind::Void => write!(f, "(none)"),
            CometTypeKind::Unkown => write!(f, "(unkown)"),
        }
    }
}

impl PartialEq for CometType {
    fn eq(&self, other: &Self) -> bool {

        let parent_type = self.cranelift_type;
        let child_type = other.cranelift_type;

        if (self.is_int() && other.is_int()) && parent_type.wider_or_equal(child_type) {
            let self_signed = match self.kind {
                CometTypeKind::Scalar(signed) => signed,
                _ => unreachable!()
            };
            let other_signed = match other.kind {
                CometTypeKind::Scalar(signed) => signed,
                _ => unreachable!()
            };

            return self_signed == other_signed;
        }

        if self.is_float() && other.is_float() {
            return true;
        }

        if self.is_array() && other.is_array() {
            let (self_elem, self_size) = match &self.kind {
                CometTypeKind::Array { base_type, size } => (base_type, size),
                _ => unreachable!()
            };
            let (other_elem, other_size) = match &other.kind {
                CometTypeKind::Array { base_type, size } => (base_type, size),
                _ => unreachable!()
            };

            return (self_elem == other_elem) && (self_size == other_size);
        }

        match &self.kind {
            CometTypeKind::Pointer(_) => { return self.kind == other.kind; },
            _ => {}
        }

        false
    }
}