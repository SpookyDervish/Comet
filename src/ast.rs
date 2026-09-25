use miette::{SourceOffset, SourceSpan};

use crate::token::Token;

#[derive(Debug, Clone)]
pub enum ASTType<'a> {
    Identifier(Box<ASTNode<'a>>),
    Function { arg_types: Vec<ASTNode<'a>>, return_type: Option<Box<ASTNode<'a>>> } ,
    Pointer(Box<ASTNode<'a>>),
    Array(Box<ASTNode<'a>>, Box<ASTNode<'a>>),
    
}

// TODO: use an arena allocator instead of heap allocating all the nodes
#[derive(Debug, Clone)]
pub enum ASTNodeType<'a> {
    Program(Vec<ASTNode<'a>>),
    Block(Vec<ASTNode<'a>>),

    FuncArgDefinition { name: Box<ASTNode<'a>>, type_: Box<ASTNode<'a>> },
    MatchNode { expressions: Vec<ASTNode<'a>>, block: Box<ASTNode<'a>> },
    StructFieldDefinition { ident: Box<ASTNode<'a>>, type_: Box<ASTNode<'a>> },
    StructField { ident: Box<ASTNode<'a>>, value: Box<ASTNode<'a>> },

    // statements
    ExpressionStatement(Box<ASTNode<'a>>),
    FuncDefinitionStatement { name: Box<ASTNode<'a>>, args: Vec<ASTNode<'a>>, return_type: Option<Box<ASTNode<'a>>>, body: Box<ASTNode<'a>> },
    ReturnStatement(Option<Box<ASTNode<'a>>>),
    AssignStatement { ident: Box<ASTNode<'a>>, type_: Option<Box<ASTNode<'a>>>, value: Box<ASTNode<'a>> },
    MatchStatement { expr: Box<ASTNode<'a>>, nodes: Vec<ASTNode<'a>>, default: Option<Box<ASTNode<'a>>> },
    IfStatement { expr: Box<ASTNode<'a>>, body: Box<ASTNode<'a>>, else_body: Option<Box<ASTNode<'a>>> },
    WhileStatement { expr: Box<ASTNode<'a>>, body: Box<ASTNode<'a>> },
    StructDefinitionStatement { ident: Box<ASTNode<'a>>, fields: Vec<ASTNode<'a>>, generics: Option<Vec<ASTNode<'a>>> },
    CompilerDirectiveStatement { directive: Box<ASTNode<'a>>, value_name: Box<ASTNode<'a>>, value_type: Box<ASTNode<'a>> },
    ImplDefStatement { struct_type: Box<ASTNode<'a>>, functions: Vec<ASTNode<'a>> },

    // expressions
    InfixExpression { left: Box<ASTNode<'a>>, op: Token<'a>, right: Box<ASTNode<'a>> },
    PrefixExpression { op: Token<'a>, right: Box<ASTNode<'a>> },
    FuncCall { left: Box<ASTNode<'a>>, args: Vec<ASTNode<'a>> },
    NewInstanceExpression { type_: Box<ASTNode<'a>>, fields: Option<Vec<ASTNode<'a>>> },
    IndexExpression { left: Box<ASTNode<'a>>, index: Box<ASTNode<'a>> },

    // literals
    IntLiteral(u64),
    FloatLiteral(f64),
    IdentifierLiteral(String),
    StringLiteral(String),
    TypeLiteral { base_type: ASTType<'a>, generic_types: Option<Vec<ASTNode<'a>>> },
    ArrayLiteral(Vec<ASTNode<'a>>)
}

#[derive(Debug, Clone)]
pub struct ASTNode<'a> {
    node_type: ASTNodeType<'a>,
    source_span: SourceSpan
}

impl <'a> ASTNode <'a> {
    pub fn new(node_type: ASTNodeType<'a>, source_span: SourceSpan) -> Self {
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

    pub fn node_type(&self) -> &ASTNodeType<'a> {
        &self.node_type
    }

    pub fn node_type_mut(&mut self) -> &mut ASTNodeType<'a> {
        &mut self.node_type
    }
}