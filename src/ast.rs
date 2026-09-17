use crate::token::Token;

// TODO: use an arena allocator instead of heap allocating all the nodes
#[derive(Debug)]
pub enum ASTNodeType {
    Program(Vec<ASTNode>),
    Block(Vec<ASTNode>),

    FuncArgDefinition { name: Box<ASTNode>, type_: Box<ASTNode> },

    // statements
    ExpressionStatement(Box<ASTNode>),
    FuncDefinitionStatement { name: Box<ASTNode>, args: Vec<ASTNode>, return_type: Option<Box<ASTNode>>, body: Box<ASTNode> },
    ReturnStatement(Option<Box<ASTNode>>),
    LetStatement { ident: Box<ASTNode>, type_: Option<Box<ASTNode>>, value: Box<ASTNode> },

    // expressions
    InfixExpression { left: Box<ASTNode>, op: Token, right: Box<ASTNode> },

    // literals
    IntLiteral(u64),
    FloatLiteral(f64),
    IdentifierLiteral(String),
    TypeLiteral(Box<ASTNode>),
}

#[derive(Debug)]
pub struct ASTNode {
    node_type: ASTNodeType
}

impl ASTNode {
    pub fn new(node_type: ASTNodeType) -> Self {
        ASTNode {
            node_type: node_type
        }
    }

    pub fn node_type(&self) -> &ASTNodeType {
        &self.node_type
    }

    pub fn node_type_mut(&mut self) -> &mut ASTNodeType {
        &mut self.node_type
    }
}