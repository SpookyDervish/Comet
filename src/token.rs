use miette::{SourceOffset, SourceSpan};

use crate::position::Position;
use crate::precedence::PrecedenceType;
use crate::range::Range;

#[derive(Debug, PartialEq, Clone)]
pub enum TokenType {
    // words
    Identifier(String),

    // keywords,
    Fun, Ret,
    Match, Default, If, Else, While,
    Struct, Init, Impl,

    // symbols
    Plus, Minus, Times, Divide, Eq,
    OpenParen, CloseParen,
    OpenCurly, CloseCurly,
    EqEq, NotEq, Gt, Lt, LtEq, GtEq,
    Not,
    Colon, Comma, ColonColon,
    Pipe, Ampersand, Caret,
    Or, And,
    Hash,
    Dot,

    // literals
    IntLiteral(String), FloatLiteral(String), StringLiteral(String)
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

            TokenType::EqEq => PrecedenceType::Equals,
            TokenType::NotEq => PrecedenceType::Equals,
            TokenType::Gt => PrecedenceType::LessGreater,
            TokenType::Lt => PrecedenceType::LessGreater,
            TokenType::GtEq => PrecedenceType::LessGreater,
            TokenType::LtEq => PrecedenceType::LessGreater,

            TokenType::Dot => PrecedenceType::Dot,
            TokenType::ColonColon => PrecedenceType::Dot,
            _ => PrecedenceType::Lowest
        }
    }

    pub fn keyword(word: &str) -> Option<Self> {
        match word {
            "fun" => Some(TokenType::Fun),
            "ret" => Some(TokenType::Ret),
            "match" => Some(TokenType::Match),
            "default" => Some(TokenType::Default),
            "if" => Some(TokenType::If),
            "else" => Some(TokenType::Else),
            "while" => Some(TokenType::While),
            "struct" => Some(TokenType::Struct),
            "init" => Some(TokenType::Init),
            "impl" => Some(TokenType::Impl),

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
pub struct Token <'a>  {
    range: Range,
    pos: Position <'a>,
    end_pos: Position <'a>,
    token_type: TokenType
}

impl <'a> Token <'a> {
    pub fn new(token_type: TokenType, range: Range, pos: Position<'a>, end_pos: Position<'a>) -> Self {
        Token {
            token_type: token_type,
            range: range,
            pos: pos,
            end_pos: end_pos
        }
    }

    pub fn source_span(&self) -> SourceSpan {
        SourceSpan::new(SourceOffset::from(self.range.start_pos()), self.range.end_pos() - self.range.start_pos())
    }

    pub fn end_span(&self) -> SourceSpan {
        SourceSpan::new(SourceOffset::from(self.range.end_pos()), 1)
    }

    pub fn pos(&self) -> &Position<'a> {
        &self.pos
    }
    pub fn end_pos(&self) -> &Position<'a> {
        &self.end_pos
    }

    pub fn token_type(&self) -> &TokenType {
        &self.token_type
    }

    pub fn range(&self) -> &Range {
        &self.range
    }
}