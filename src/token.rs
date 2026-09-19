use crate::precedence::PrecedenceType;
use crate::range::Range;

#[derive(Debug, PartialEq, Clone)]
pub enum TokenType {
    // words
    Identifier(String),

    // keywords,
    Fun, Ret, Match,

    // symbols
    Plus, Minus, Times, Divide, Eq,
    OpenParen, CloseParen,
    OpenCurly, CloseCurly,
    Colon, Comma, ColonColon,
    BitwiseOr, BitwiseAnd,
    Or, And,
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
            TokenType::OpenParen => PrecedenceType::Call,
            _ => PrecedenceType::Lowest
        }
    }

    pub fn keyword(word: &str) -> Option<Self> {
        match word {
            "fun" => Some(TokenType::Fun),
            "ret" => Some(TokenType::Ret),
            "match" => Some(TokenType::Match),

            "or" => Some(TokenType::Or),
            "and" => Some(TokenType::And),
            _ => None
        }
    }

    pub fn as_identifier(&self) -> Option<&String> {
        match self {
            TokenType::Identifier(value) => Some(&value),
            _ => None 
        }
    }
}

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