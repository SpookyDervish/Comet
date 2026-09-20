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

    pub fn lex_word(&mut self) -> Token {
        let mut word_string = String::new();
        let mut range = Range::start(&self.pos);

        while self.current_char != '\0' && (self.current_char.is_alphanumeric() || self.current_char == '_') {
            word_string.push(self.current_char);
            self.consume();
        }

        range.end(&self.pos);

        let keyword = TokenType::keyword(&word_string);
        if keyword.is_some() {
            Token::new(keyword.unwrap(), range)
        } else {
            Token::new(TokenType::Identifier(word_string), range)
        }
    }

    pub fn lex_number(&mut self) -> Token {
        let mut number_string = String::new();
        let mut dot_count = 0;

        let mut range = Range::start(&self.pos);

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

        if dot_count > 0 {
            Token::new(TokenType::FloatLiteral(number_string), range)
        } else {
            Token::new(TokenType::IntLiteral(number_string), range)
        }
    }

    pub fn lex(&mut self) -> Result<Vec<Token>, String> {
        let mut tokens: Vec<Token> = Vec::new();

        // get current and peak character
        self.consume();

        while self.current_char != '\0' {
            match self.current_char {
                ' ' | '\t' => self.consume(), // ignore whitespace
                '\n' => {
                    self.consume();
                }

                '+' => {
                    tokens.push(Token::new(TokenType::Plus, Range::from(&self.pos)));
                    self.consume();
                }
                '-' => {
                    tokens.push(Token::new(TokenType::Minus, Range::from(&self.pos)));
                    self.consume();
                }
                '*' => {
                    tokens.push(Token::new(TokenType::Times, Range::from(&self.pos)));
                    self.consume();
                }
                '/' => {
                    tokens.push(Token::new(TokenType::Divide, Range::from(&self.pos)));
                    self.consume();
                }
                '=' => {
                    let mut range = Range::from(&self.pos);
                    self.consume();

                    if self.current_char != '=' {
                        tokens.push(Token::new(TokenType::Eq, range));
                    } else {
                        range.end(&self.pos);
                        tokens.push(Token::new(TokenType::EqEq, range));
                        self.consume();
                    }
                    
                    
                }
                '!' => {
                    let mut range = Range::from(&self.pos);
                    self.consume();

                    if self.current_char != '=' {
                        tokens.push(Token::new(TokenType::Not, range));
                    } else {
                        range.end(&self.pos);
                        tokens.push(Token::new(TokenType::NotEq, range));
                        self.consume();
                    }
                    
                    
                }
                '>' => {
                    let mut range = Range::from(&self.pos);
                    self.consume();

                    if self.current_char != '=' {
                        tokens.push(Token::new(TokenType::Gt, range));
                    } else {
                        range.end(&self.pos);
                        tokens.push(Token::new(TokenType::GtEq, range));
                        self.consume();
                    }
                    
                    
                }
                '<' => {
                    let mut range = Range::from(&self.pos);
                    self.consume();

                    if self.current_char != '=' {
                        tokens.push(Token::new(TokenType::Lt, range));
                    } else {
                        range.end(&self.pos);
                        tokens.push(Token::new(TokenType::LtEq, range));
                        self.consume();
                    }
                    
                    
                }
                '(' => {
                    tokens.push(Token::new(TokenType::OpenParen, Range::from(&self.pos)));
                    self.consume();
                }
                ')' => {
                    tokens.push(Token::new(TokenType::CloseParen, Range::from(&self.pos)));
                    self.consume();
                }
                '{' => {
                    tokens.push(Token::new(TokenType::OpenCurly, Range::from(&self.pos)));
                    self.consume();
                }
                '}' => {
                    tokens.push(Token::new(TokenType::CloseCurly, Range::from(&self.pos)));
                    self.consume();
                }
                ':' => {
                    let mut range = Range::from(&self.pos);
                    self.consume();

                    if self.current_char != ':' {
                        tokens.push(Token::new(TokenType::Colon, range));
                    } else {
                        range.end(&self.pos);
                        tokens.push(Token::new(TokenType::ColonColon, range));
                        self.consume();
                    }
                    
                    
                }
                ',' => {
                    tokens.push(Token::new(TokenType::Comma, Range::from(&self.pos)));
                    self.consume();
                }
                '.' => {
                    tokens.push(Token::new(TokenType::Dot, Range::from(&self.pos)));
                    self.consume();
                }
                '|' => {
                    tokens.push(Token::new(TokenType::BitwiseOr, Range::from(&self.pos)));
                    self.consume();
                }
                '&' => {
                    tokens.push(Token::new(TokenType::BitwiseAnd, Range::from(&self.pos)));
                    self.consume();
                }

                _ => {
                    if self.current_char.is_ascii_digit() || self.current_char == '.' {
                        tokens.push(self.lex_number());
                    } else if self.current_char.is_alphanumeric() || self.current_char == '_' {
                        tokens.push(self.lex_word());
                    } else {
                        return Err(format!("Unexpected character \'{}\'", self.current_char));
                    }
                }
            }
        }

        Ok(tokens)
    }
}
