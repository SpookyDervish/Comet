use std::collections::HashMap;

use cranelift_frontend::Variable;
use cranelift_module::FuncId;

use crate::comet_type::CometType;

pub enum CometVarType {
    Local(Variable),
    FuncArg(usize) // stores func arg index
}

pub struct CometVariable {
    pub type_: CometType,
    pub var_type: CometVarType,
    pub mutable: bool
}

pub struct ScopeFrame <'a> {
    pub variables: HashMap<&'a str, CometVariable>,
    pub types: HashMap<&'a str, CometType>,
    pub functions: HashMap<&'a str, FuncId>
}

impl <'a> ScopeFrame <'a> {
    pub fn new() -> Self {
        ScopeFrame { variables: HashMap::new(), types: HashMap::new(), functions: HashMap::new() }
    }
}