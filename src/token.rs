use crate::range::Range;

#[derive(Debug)]
pub enum TokenType {
    // words
    Identifier(String), Keyword(String),

    // symbols
    Plus, Minus, Times, Divide, Eq,
    OpenParen, CloseParen,
    OpenCurly, CloseCurly,

    // literals
    IntLiteral(String), FloatLiteral(String)
}

pub const COMET_KEYWORDS: [&str; 1] = [
    "var"
];

#[derive(Debug)]
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
}