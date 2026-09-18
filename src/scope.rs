use std::collections::HashMap;

use cranelift_frontend::Variable;

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
    pub types: HashMap<&'a str, CometType>
}

impl <'a> ScopeFrame <'a> {
    pub fn new() -> Self {
        ScopeFrame { variables: HashMap::new(), types: HashMap::new() }
    }
}