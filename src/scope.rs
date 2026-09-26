use std::collections::HashMap;

use cranelift_frontend::Variable;
use cranelift_module::FuncId;

use crate::{ast::ASTNode, comet_type::CometType};

#[derive(Debug)]
pub enum CometVarType {
    Local(Variable),
    FuncArg(usize), // stores func arg index
    External(FuncId)
}

#[derive(Debug)]
pub struct CometVariable {
    pub type_: CometType,
    pub var_type: CometVarType,
    pub mutable: bool,
    pub function_id: Option<FuncId>
}

pub struct ScopeFrame<'a> {
    pub variables: HashMap<String, CometVariable>,
    pub types: HashMap<String, CometType>,
    pub generics: HashMap<String, ASTNode<'a>>
}

impl <'a> ScopeFrame <'a> {
    pub fn new() -> Self {
        ScopeFrame {
            variables: HashMap::new(),
            types: HashMap::new(),
            generics: HashMap::new()
        }
    }
}