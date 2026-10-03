use std::collections::HashMap;

use cranelift_frontend::Variable;
use cranelift_module::FuncId;

use crate::{ast::ASTNode, comet_type::CometType};

#[derive(Debug)]
pub enum CometVarType <'a> {
    Local(Variable),
    FuncArg(usize), // stores func arg index
    External(FuncId),
    Module(ScopeFrame<'a>)
}

#[derive(Debug)]
pub struct CometVariable<'a> {
    pub type_: CometType,
    pub var_type: CometVarType<'a>,
    pub mutable: bool,
    pub function_id: Option<FuncId>
}

#[derive(Debug)]
pub struct ScopeFrame<'a> {
    pub variables: HashMap<String, CometVariable<'a>>,
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