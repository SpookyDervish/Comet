use crate::token::Token;

// TODO: use an arena allocator instead of heap allocating all the nodes
#[derive(Debug)]
pub enum ASTNodeType {
    Program(Vec<ASTNode>),
    Block(Vec<ASTNode>),

    FuncArgDefinition { name: Box<ASTNode>, type_: Box<ASTNode> },
    MatchNode { expressions: Vec<ASTNode>, block: Box<ASTNode> },
    StructFieldDefinition { ident: Box<ASTNode>, type_: Box<ASTNode> },

    // statements
    ExpressionStatement(Box<ASTNode>),
    FuncDefinitionStatement { name: Box<ASTNode>, args: Vec<ASTNode>, return_type: Option<Box<ASTNode>>, body: Box<ASTNode> },
    ReturnStatement(Option<Box<ASTNode>>),
    AssignStatement { ident: Box<ASTNode>, type_: Option<Box<ASTNode>>, value: Box<ASTNode> },
    MatchStatement { expr: Box<ASTNode>, nodes: Vec<ASTNode>, default: Option<Box<ASTNode>> },
    IfStatement { expr: Box<ASTNode>, body: Box<ASTNode>, else_body: Option<Box<ASTNode>> },
    WhileStatement { expr: Box<ASTNode>, body: Box<ASTNode> },
    StructDefinitionStatement { ident: Box<ASTNode>, fields: Vec<ASTNode> },

    // expressions
    InfixExpression { left: Box<ASTNode>, op: Token, right: Box<ASTNode> },
    FuncCall { left: Box<ASTNode>, args: Vec<ASTNode> },

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