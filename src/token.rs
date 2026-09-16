use crate::precedence::PrecedenceType;
use crate::range::Range;

#[derive(Debug, PartialEq, Clone)]
pub enum TokenType {
    // words
    Identifier(String), Keyword(String),

    // symbols
    Plus, Minus, Times, Divide, Eq,
    OpenParen, CloseParen,
    OpenCurly, CloseCurly,
    Dot,

    // literals
    IntLiteral(String), FloatLiteral(String),

    Max
}

impl TokenType {
    // const fn so its evaluated at compile time
    pub const fn precedence(&self) -> PrecedenceType {
        match self {
            TokenType::Plus => PrecedenceType::Sum,
            TokenType::Minus => PrecedenceType::Sum,
            TokenType::Times => PrecedenceType::Product,
            TokenType::Divide => PrecedenceType::Product,
            _ => PrecedenceType::Lowest
        }
    }
}

pub const COMET_KEYWORDS: [&str; 3] = [
    "func",
    "struct",
    "return"
];

#[derive(Debug, Clone)]
pub struct Token {
    range: Range,
    token_type: TokenType
}

impl Token {
    pub fn new(token_type: TokenType, range: Range) -> Self {
        Token {
            token_type: token_type,
            range: range
        }
    }

    pub fn token_type(&self) -> &TokenType {
        &self.token_type
    }

    pub fn range(&self) -> &Range {
        &self.range
    }
}