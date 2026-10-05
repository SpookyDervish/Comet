use miette::{SourceOffset, SourceSpan};

use crate::token::Token;

#[derive(Debug, Clone)]
pub enum ASTType {
    Identifier(Box<ASTNode>),
    Function { arg_types: Vec<ASTNode>, return_type: Option<Box<ASTNode>> } ,
    Pointer(Box<ASTNode>),
    Array(Box<ASTNode>, Box<ASTNode>),
    Qualified(Vec<ASTNode>),
    ModuleQualified(Vec<ASTNode>)
    
}

#[derive(Debug, Clone)]
pub enum MatchPattern {
    Expression(ASTNode),
    Binding(ASTNode),
    Variant { path: Vec<ASTNode>, fields: Vec<MatchPattern> }
}

// TODO: use an arena allocator instead of heap allocating all the nodes
#[derive(Debug, Clone)]
pub enum ASTNodeType {
    Program(Vec<ASTNode>),
    Block(Vec<ASTNode>),

    FuncArgDefinition { name: Box<ASTNode>, type_: Box<ASTNode> },
    MatchNode { expressions: Vec<MatchPattern>, block: Box<ASTNode> },
    StructFieldDefinition { ident: Box<ASTNode>, type_: Box<ASTNode> },
    StructField { ident: Box<ASTNode>, value: Box<ASTNode> },
    UnionItemDefinition { ident: Box<ASTNode>, names: Vec<ASTNode>, types: Vec<ASTNode> },
    QualifierNode { ident: Box<ASTNode>, generics: Option<Vec<ASTNode>> },
    ModuleQualifierNode { ident: Box<ASTNode>, generics: Option<Vec<ASTNode>> },

    // statements
    ExpressionStatement(Box<ASTNode>),
    FuncDefinitionStatement { name: Box<ASTNode>, args: Vec<ASTNode>, return_type: Option<Box<ASTNode>>, body: Box<ASTNode> },
    ReturnStatement(Option<Box<ASTNode>>),
    AssignStatement { ident: Box<ASTNode>, type_: Option<Box<ASTNode>>, value: Box<ASTNode> },
    MatchStatement { expr: Box<ASTNode>, nodes: Vec<ASTNode>, default: Option<Box<ASTNode>> },
    IfStatement { expr: Box<ASTNode>, body: Box<ASTNode>, else_body: Option<Box<ASTNode>> },
    WhileStatement { expr: Box<ASTNode>, body: Box<ASTNode> },
    StructDefinitionStatement { ident: Box<ASTNode>, fields: Vec<ASTNode>, generics: Option<Vec<ASTNode>> },
    UnionDefinitionStatement { ident: Box<ASTNode>, fields: Vec<ASTNode>, generics: Option<Vec<ASTNode>> },
    CompilerDirectiveStatement { directive: Box<ASTNode>, value_name: Box<ASTNode>, value_type: Box<ASTNode> },
    ImplDefStatement { struct_type: Box<ASTNode>, functions: Vec<ASTNode>, generics: Option<Vec<ASTNode>> },
    BringStatement { path: Vec<ASTNode>, as_: Option<Box<ASTNode>> },

    // expressions
    InfixExpression { left: Box<ASTNode>, op: Token, right: Box<ASTNode> },
    PrefixExpression { op: Token, right: Box<ASTNode> },
    FuncCall { left: Box<ASTNode>, args: Vec<ASTNode> },
    NewInstanceExpression { type_: Box<ASTNode>, fields: Option<Vec<ASTNode>> },
    IndexExpression { left: Box<ASTNode>, index: Box<ASTNode> },

    // literals
    IntLiteral(u64),
    FloatLiteral(f64),
    IdentifierLiteral(String),
    StringLiteral(String),
    TypeLiteral { base_type: ASTType, generic_types: Option<Vec<ASTNode>> },
    ArrayLiteral(Vec<ASTNode>)
}

#[derive(Debug, Clone)]
pub struct ASTNode {
    node_type: ASTNodeType,
    source_span: SourceSpan
}

impl ASTNode {
    pub fn new(node_type: ASTNodeType, source_span: SourceSpan) -> Self {
        ASTNode {
            node_type: node_type,
            source_span: source_span
        }
    }

    pub fn start_pos(&self) -> SourceOffset {
        self.source_span.offset().into()
    }

    pub fn end_pos(&self) -> SourceOffset {
        (self.source_span.offset() + self.source_span.len()).into()
    }

    pub fn get_span(&self, to: &ASTNode) -> SourceSpan {
        SourceSpan::new(self.start_pos(), to.end_pos().offset() - self.start_pos().offset())
    }

    pub fn source_span(&self) -> SourceSpan {
        self.source_span.clone()
    }

    pub fn node_type(&self) -> &ASTNodeType {
        &self.node_type
    }

    pub fn node_type_mut(&mut self) -> &mut ASTNodeType {
        &mut self.node_type
    }
}