use crate::position::Position;
use crate::token::{self, Token, TokenType};
use crate::range::Range;

pub struct Lexer <'file> {
    current_char: char,
    pos: Position <'file>
}

impl <'file> Lexer <'file> {
    pub fn new(file_name: &'file str, source_code: &'file str) -> Self {
        Lexer { 
            current_char: '\0',

            pos: Position::new(file_name, source_code)
         }
    }

    pub fn consume(&mut self) {
        
        self.current_char = if self.pos.idx() < self.pos.source().len() { self.pos.source().chars().nth(self.pos.idx()).unwrap() } else { '\0' };
        
        if self.current_char != '\0' {
            self.pos.advance();
        }
    }

    pub fn lex_word(&mut self) -> Token {
        let mut word_string = String::new();
        let mut range = Range::start(&self.pos);

        while self.current_char != '\0' && self.current_char.is_alphanumeric() {
            word_string.push(self.current_char);
            self.consume();
        }

        range.end(&self.pos);

        if token::COMET_KEYWORDS.contains(&word_string.as_str()) {
            Token::new(TokenType::Keyword(word_string), range)
        } else {
            Token::new(TokenType::Identifier(word_string), range)
        }
    }

    pub fn lex_number(&mut self) -> Token {
        let mut number_string = String::new();
        let mut dot_count = 0;

        let mut range = Range::start(&self.pos);

        while self.current_char != '\0' && (self.current_char.is_ascii_digit() || self.current_char == '.') {
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

    pub fn lex(&mut self) -> Result<Vec<Token>, ()> {
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
                },
                '-' => {
                    tokens.push(Token::new(TokenType::Minus, Range::from(&self.pos)));
                    self.consume();
                },
                '*' => {
                    tokens.push(Token::new(TokenType::Times, Range::from(&self.pos)));
                    self.consume();
                },
                '/' => {
                    tokens.push(Token::new(TokenType::Divide, Range::from(&self.pos)));
                    self.consume();
                },
                '=' => {
                    tokens.push(Token::new(TokenType::Eq, Range::from(&self.pos)));
                    self.consume();
                },
                '(' => {
                    tokens.push(Token::new(TokenType::OpenParen, Range::from(&self.pos)));
                    self.consume();
                },
                ')' => {
                    tokens.push(Token::new(TokenType::CloseParen, Range::from(&self.pos)));
                    self.consume();
                },
                '{' => {
                    tokens.push(Token::new(TokenType::OpenCurly, Range::from(&self.pos)));
                    self.consume();
                },
                '}' => {
                    tokens.push(Token::new(TokenType::CloseCurly, Range::from(&self.pos)));
                    self.consume();
                },

                _ => {
                    if self.current_char.is_ascii_digit() || self.current_char == '.' {
                        tokens.push(self.lex_number());
                    } else if self.current_char.is_alphanumeric() {
                        tokens.push(self.lex_word());
                    } else {
                        println!("unrecognized token '{}'", self.current_char);
                        todo!();
                    }
                }
            }

            
        }

        return Ok(tokens);
    }
}