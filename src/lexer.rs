use miette::{NamedSource, SourceSpan};

use crate::comet_error::SyntaxError;
use crate::position::Position;
use crate::range::Range;
use crate::token::{Token, TokenType};

pub struct Lexer<'file> {
    current_char: char,
    pos: Position<'file>,
}

impl<'file> Lexer<'file> {
    pub fn new(file_name: &'file str, source_code: &'file str) -> Self {
        Lexer {
            current_char: '\0',

            pos: Position::new(file_name, source_code),
        }
    }

    pub fn consume(&mut self) {
        self.current_char = if self.pos.idx() < self.pos.source().len() {
            self.pos.source().chars().nth(self.pos.idx()).unwrap()
        } else {
            '\0'
        };

        if self.current_char != '\0' {
            self.pos.advance();
        }
    }

    pub fn lex_word(&mut self) -> Token<'file> {
        let mut word_string = String::new();
        let mut range = Range::start(&self.pos);
        let start_pos = self.pos.clone();

        while self.current_char != '\0' && (self.current_char.is_alphanumeric() || self.current_char == '_') {
            word_string.push(self.current_char);
            self.consume();
        }

        range.end(&self.pos);
        let end_pos = self.pos.clone();

        let keyword = TokenType::keyword(&word_string);
        if keyword.is_some() {
            Token::new(keyword.unwrap(), range, start_pos, end_pos)
        } else {
            Token::new(TokenType::Identifier(word_string), range, start_pos, end_pos)
        }
    }

    pub fn lex_string(&mut self) -> Token<'file> {
        let mut word_string = String::new();
        let mut range = Range::start(&self.pos);
        let start_pos = self.pos.clone();

        self.consume();

        while self.current_char != '"' {
            word_string.push(self.current_char);
            self.consume();
        }

        range.end(&self.pos);
        let end_pos = self.pos.clone();

        self.consume();

        Token::new(TokenType::StringLiteral(word_string), range, start_pos, end_pos)
    }

    pub fn lex_number(&mut self) -> Token<'file> {
        let mut number_string = String::new();
        let mut dot_count = 0;

        let mut range = Range::start(&self.pos);
        let start_pos = self.pos.clone();

        while self.current_char != '\0'
            && (self.current_char.is_ascii_digit() || self.current_char == '.')
        {
            if self.current_char == '.' {
                if dot_count == 1 {
                    break;
                }

                dot_count += 1;
                number_string.push('.')
            } else {
                number_string.push(self.current_char)
            }

            self.consume();
        }

        range.end(&self.pos);
        let end_pos = self.pos.clone();

        if dot_count > 0 {
            Token::new(TokenType::FloatLiteral(number_string), range, start_pos, end_pos)
        } else {
            Token::new(TokenType::IntLiteral(number_string), range, start_pos, end_pos)
        }
    }

    pub fn lex(&mut self) -> miette::Result<Vec<Token<'file>>> {
        let mut tokens: Vec<Token<'file>> = Vec::new();

        // get current and peak character
        self.consume();

        while self.current_char != '\0' {
            match self.current_char {
                ' ' | '\t' => self.consume(), // ignore whitespace
                '\n' => {
                    self.consume();
                }

                '"' => {
                    tokens.push(self.lex_string());
                }

                '+' => {
                    tokens.push(Token::new(TokenType::Plus, Range::from(&self.pos), self.pos.clone(), self.pos.clone()));
                    self.consume();
                }
                '-' => {
                    tokens.push(Token::new(TokenType::Minus, Range::from(&self.pos), self.pos.clone(), self.pos.clone()));
                    self.consume();
                }
                '*' => {
                    let mut range = Range::from(&self.pos);
                    self.consume();

                    if self.current_char != '*' {
                        tokens.push(Token::new(TokenType::Times, range, self.pos.clone(), self.pos.clone()));
                    } else {
                        range.end(&self.pos);
                        tokens.push(Token::new(TokenType::StarStar, range, self.pos.clone(), self.pos.clone()));
                        self.consume();
                    }
                    
                    
                }
                '/' => {
                    tokens.push(Token::new(TokenType::Divide, Range::from(&self.pos), self.pos.clone(), self.pos.clone()));
                    self.consume();
                }
                '%' => {
                    tokens.push(Token::new(TokenType::Modulo, Range::from(&self.pos), self.pos.clone(), self.pos.clone()));
                    self.consume();
                }
                '=' => {
                    let mut range = Range::from(&self.pos);
                    self.consume();

                    if self.current_char == '>' {
                        range.end(&self.pos);
                        tokens.push(Token::new(TokenType::Arrow, range, self.pos.clone(), self.pos.clone()));
                        self.consume();
                    } else if self.current_char == '=' {
                        range.end(&self.pos);
                        tokens.push(Token::new(TokenType::EqEq, range, self.pos.clone(), self.pos.clone()));
                        self.consume();
                    } else {
                        tokens.push(Token::new(TokenType::Eq, range, self.pos.clone(), self.pos.clone()));
                    }
                    
                    
                }
                '!' => {
                    let mut range = Range::from(&self.pos);
                    self.consume();

                    if self.current_char != '=' {
                        range.end(&self.pos);
                        return Err(SyntaxError {
                            span: range.source_span(),
                            src: NamedSource::new(self.pos.file_name(), String::from(self.pos.source())),
                            text: "Expected '=' after '!'".to_string()
                        }.into());
                    } else {
                        range.end(&self.pos);
                        tokens.push(Token::new(TokenType::NotEq, range, self.pos.clone(), self.pos.clone()));
                        self.consume();
                    }
                    
                    
                }
                '~' => {
                    tokens.push(Token::new(TokenType::Tilde, Range::from(&self.pos), self.pos.clone(), self.pos.clone()));
                    self.consume();
                }
                '>' => {
                    let mut range = Range::from(&self.pos);
                    self.consume();

                    if self.current_char != '=' {
                        tokens.push(Token::new(TokenType::Gt, range, self.pos.clone(), self.pos.clone()));
                    } else {
                        range.end(&self.pos);
                        tokens.push(Token::new(TokenType::GtEq, range, self.pos.clone(), self.pos.clone()));
                        self.consume();
                    }
                    
                    
                }
                '<' => {
                    let mut range = Range::from(&self.pos);
                    self.consume();

                    if self.current_char != '=' {
                        tokens.push(Token::new(TokenType::Lt, range, self.pos.clone(), self.pos.clone()));
                    } else {
                        range.end(&self.pos);
                        tokens.push(Token::new(TokenType::LtEq, range, self.pos.clone(), self.pos.clone()));
                        self.consume();
                    }
                    
                    
                }
                '(' => {
                    tokens.push(Token::new(TokenType::OpenParen, Range::from(&self.pos), self.pos.clone(), self.pos.clone()));
                    self.consume();
                }
                ')' => {
                    tokens.push(Token::new(TokenType::CloseParen, Range::from(&self.pos), self.pos.clone(), self.pos.clone()));
                    self.consume();
                }
                '{' => {
                    tokens.push(Token::new(TokenType::OpenCurly, Range::from(&self.pos), self.pos.clone(), self.pos.clone()));
                    self.consume();
                }
                '}' => {
                    tokens.push(Token::new(TokenType::CloseCurly, Range::from(&self.pos), self.pos.clone(), self.pos.clone()));
                    self.consume();
                }
                '[' => {
                    tokens.push(Token::new(TokenType::OpenSquare, Range::from(&self.pos), self.pos.clone(), self.pos.clone()));
                    self.consume();
                }
                ']' => {
                    tokens.push(Token::new(TokenType::CloseSquare, Range::from(&self.pos), self.pos.clone(), self.pos.clone()));
                    self.consume();
                }
                ':' => {
                    let mut range = Range::from(&self.pos);
                    self.consume();

                    if self.current_char != ':' {
                        tokens.push(Token::new(TokenType::Colon, range, self.pos.clone(), self.pos.clone()));
                    } else {
                        range.end(&self.pos);
                        tokens.push(Token::new(TokenType::ColonColon, range, self.pos.clone(), self.pos.clone()));
                        self.consume();
                    }
                    
                    
                }
                ',' => {
                    tokens.push(Token::new(TokenType::Comma, Range::from(&self.pos), self.pos.clone(), self.pos.clone()));
                    self.consume();
                }
                '.' => {
                    tokens.push(Token::new(TokenType::Dot, Range::from(&self.pos), self.pos.clone(), self.pos.clone()));
                    self.consume();
                }
                '|' => {
                    tokens.push(Token::new(TokenType::Pipe, Range::from(&self.pos), self.pos.clone(), self.pos.clone()));
                    self.consume();
                }
                '&' => {
                    tokens.push(Token::new(TokenType::Ampersand, Range::from(&self.pos), self.pos.clone(), self.pos.clone()));
                    self.consume();
                }
                '^' => {
                    tokens.push(Token::new(TokenType::Caret, Range::from(&self.pos), self.pos.clone(), self.pos.clone()));
                    self.consume();
                }
                '#' => {
                    tokens.push(Token::new(TokenType::Hash, Range::from(&self.pos), self.pos.clone(), self.pos.clone()));
                    self.consume();
                }

                _ => {
                    if self.current_char.is_ascii_digit() || self.current_char == '.' {
                        tokens.push(self.lex_number());
                    } else if self.current_char.is_alphanumeric() || self.current_char == '_' {
                        tokens.push(self.lex_word());
                    } else {
                        return Err(SyntaxError {
                            src: NamedSource::new(self.pos.file_name(), String::from(self.pos.source())),
                            span: SourceSpan::new(self.pos.idx().into(), 1),
                            text: format!("Unexpected character \'{}\'", self.current_char)
                        }.into());
                    }
                }
            }
        }

        Ok(tokens)
    }
}
