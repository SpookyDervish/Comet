use crate::token::Token;

// TODO: use an arena allocator instead of heap allocating all the nodes
#[derive(Debug)]
pub enum ASTNodeType {
    Program(Vec<ASTNode>),

    // statements
    ExpressionStatement(Box<ASTNode>),

    // expressions
    InfixExpression { left: Box<ASTNode>, right: Box<ASTNode>, op: Token },

    // literals
    IntLiteral(u64),
    FloatLiteral(f64)
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