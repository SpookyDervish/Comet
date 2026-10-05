use std::rc::Rc;

use miette::{NamedSource, SourceSpan};

use crate::comet_error::SyntaxError;
use crate::position::{Pos, Position, SourceFile};
use crate::range::Range;
use crate::token::{Token, TokenType};

pub struct Lexer<'a> {
    current_char: char,
    pos: Position<'a>,
    file: Rc<SourceFile>,
}

impl<'a> Lexer<'a> {
    pub fn new(file_name: &'a str, source_code: &'a str) -> Self {
        Lexer {
            current_char: '\0',

            pos: Position::new(file_name, source_code),
            file: SourceFile::shared(file_name.to_string(), source_code.to_string()),
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
        let mut range = Range::start(Pos::from(&self.pos));
        let start_pos = Pos::from(&self.pos);

        while self.current_char != '\0' && (self.current_char.is_alphanumeric() || self.current_char == '_') {
            word_string.push(self.current_char);
            self.consume();
        }

        range.end(Pos::from(&self.pos));
        let end_pos = Pos::from(&self.pos);

        let keyword = TokenType::keyword(&word_string);
        if keyword.is_some() {
            Token::new(keyword.unwrap(), range, start_pos, end_pos, self.file.clone())
        } else {
            Token::new(TokenType::Identifier(word_string), range, start_pos, end_pos, self.file.clone())
        }
    }

    pub fn lex_string(&mut self) -> Token {
        let mut word_string = String::new();
        let mut range = Range::start(Pos::from(&self.pos));
        let start_pos = Pos::from(&self.pos);

        self.consume();

        while self.current_char != '"' {
            word_string.push(self.current_char);
            self.consume();
        }

        range.end(Pos::from(&self.pos));
        let end_pos = Pos::from(&self.pos);

        self.consume();

        Token::new(TokenType::StringLiteral(word_string), range, start_pos, end_pos, self.file.clone())
    }

    pub fn lex_number(&mut self) -> Token {
        let mut number_string = String::new();
        let mut dot_count = 0;

        let mut range = Range::start(Pos::from(&self.pos));
        let start_pos = Pos::from(&self.pos);

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

        range.end(Pos::from(&self.pos));
        let end_pos = Pos::from(&self.pos);

        if dot_count > 0 {
            Token::new(TokenType::FloatLiteral(number_string), range, start_pos, end_pos, self.file.clone())
        } else {
            Token::new(TokenType::IntLiteral(number_string), range, start_pos, end_pos, self.file.clone())
        }
    }

    pub fn lex(&mut self) -> miette::Result<Vec<Token>> {
        let mut tokens: Vec<Token> = Vec::new();

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
                    tokens.push(Token::new(TokenType::Plus, Range::from(Pos::from(&self.pos)), Pos::from(&self.pos), Pos::from(&self.pos), self.file.clone()));
                    self.consume();
                }
                '-' => {
                    tokens.push(Token::new(TokenType::Minus, Range::from(Pos::from(&self.pos)), Pos::from(&self.pos), Pos::from(&self.pos), self.file.clone()));
                    self.consume();
                }
                '*' => {
                    let mut range = Range::from(Pos::from(&self.pos));
                    self.consume();

                    if self.current_char != '*' {
                        tokens.push(Token::new(TokenType::Times, range, Pos::from(&self.pos), Pos::from(&self.pos), self.file.clone()));
                    } else {
                        range.end(Pos::from(&self.pos));
                        tokens.push(Token::new(TokenType::StarStar, range, Pos::from(&self.pos), Pos::from(&self.pos), self.file.clone()));
                        self.consume();
                    }
                    
                    
                }
                '/' => {
                    tokens.push(Token::new(TokenType::Divide, Range::from(Pos::from(&self.pos)), Pos::from(&self.pos), Pos::from(&self.pos), self.file.clone()));
                    self.consume();
                }
                '%' => {
                    tokens.push(Token::new(TokenType::Modulo, Range::from(Pos::from(&self.pos)), Pos::from(&self.pos), Pos::from(&self.pos), self.file.clone()));
                    self.consume();
                }
                '=' => {
                    let mut range = Range::from(Pos::from(&self.pos));
                    self.consume();

                    if self.current_char == '>' {
                        range.end(Pos::from(&self.pos));
                        tokens.push(Token::new(TokenType::Arrow, range, Pos::from(&self.pos), Pos::from(&self.pos), self.file.clone()));
                        self.consume();
                    } else if self.current_char == '=' {
                        range.end(Pos::from(&self.pos));
                        tokens.push(Token::new(TokenType::EqEq, range, Pos::from(&self.pos), Pos::from(&self.pos), self.file.clone()));
                        self.consume();
                    } else {
                        tokens.push(Token::new(TokenType::Eq, range, Pos::from(&self.pos), Pos::from(&self.pos), self.file.clone()));
                    }
                    
                    
                }
                '!' => {
                    let mut range = Range::from(Pos::from(&self.pos));
                    self.consume();

                    if self.current_char != '=' {
                        range.end(Pos::from(&self.pos));
                        return Err(SyntaxError {
                            span: range.source_span(),
                            src: NamedSource::new(self.pos.file_name(), String::from(self.pos.source())),
                            text: "Expected '=' after '!'".to_string()
                        }.into());
                    } else {
                        range.end(Pos::from(&self.pos));
                        tokens.push(Token::new(TokenType::NotEq, range, Pos::from(&self.pos), Pos::from(&self.pos), self.file.clone()));
                        self.consume();
                    }
                    
                    
                }
                '~' => {
                    tokens.push(Token::new(TokenType::Tilde, Range::from(Pos::from(&self.pos)), Pos::from(&self.pos), Pos::from(&self.pos), self.file.clone()));
                    self.consume();
                }
                '>' => {
                    let mut range = Range::from(Pos::from(&self.pos));
                    self.consume();

                    if self.current_char != '=' {
                        tokens.push(Token::new(TokenType::Gt, range, Pos::from(&self.pos), Pos::from(&self.pos), self.file.clone()));
                    } else {
                        range.end(Pos::from(&self.pos));
                        tokens.push(Token::new(TokenType::GtEq, range, Pos::from(&self.pos), Pos::from(&self.pos), self.file.clone()));
                        self.consume();
                    }
                    
                    
                }
                '<' => {
                    let mut range = Range::from(Pos::from(&self.pos));
                    self.consume();

                    if self.current_char != '=' {
                        tokens.push(Token::new(TokenType::Lt, range, Pos::from(&self.pos), Pos::from(&self.pos), self.file.clone()));
                    } else {
                        range.end(Pos::from(&self.pos));
                        tokens.push(Token::new(TokenType::LtEq, range, Pos::from(&self.pos), Pos::from(&self.pos), self.file.clone()));
                        self.consume();
                    }
                    
                    
                }
                '(' => {
                    tokens.push(Token::new(TokenType::OpenParen, Range::from(Pos::from(&self.pos)), Pos::from(&self.pos), Pos::from(&self.pos), self.file.clone()));
                    self.consume();
                }
                ')' => {
                    tokens.push(Token::new(TokenType::CloseParen, Range::from(Pos::from(&self.pos)), Pos::from(&self.pos), Pos::from(&self.pos), self.file.clone()));
                    self.consume();
                }
                '{' => {
                    tokens.push(Token::new(TokenType::OpenCurly, Range::from(Pos::from(&self.pos)), Pos::from(&self.pos), Pos::from(&self.pos), self.file.clone()));
                    self.consume();
                }
                '}' => {
                    tokens.push(Token::new(TokenType::CloseCurly, Range::from(Pos::from(&self.pos)), Pos::from(&self.pos), Pos::from(&self.pos), self.file.clone()));
                    self.consume();
                }
                '[' => {
                    tokens.push(Token::new(TokenType::OpenSquare, Range::from(Pos::from(&self.pos)), Pos::from(&self.pos), Pos::from(&self.pos), self.file.clone()));
                    self.consume();
                }
                ']' => {
                    tokens.push(Token::new(TokenType::CloseSquare, Range::from(Pos::from(&self.pos)), Pos::from(&self.pos), Pos::from(&self.pos), self.file.clone()));
                    self.consume();
                }
                '@' => {
                    tokens.push(Token::new(TokenType::At, Range::from(Pos::from(&self.pos)), Pos::from(&self.pos), Pos::from(&self.pos), self.file.clone()));
                    self.consume();
                }
                ':' => {
                    let mut range = Range::from(Pos::from(&self.pos));
                    self.consume();

                    if self.current_char != ':' {
                        tokens.push(Token::new(TokenType::Colon, range, Pos::from(&self.pos), Pos::from(&self.pos), self.file.clone()));
                    } else {
                        range.end(Pos::from(&self.pos));
                        tokens.push(Token::new(TokenType::ColonColon, range, Pos::from(&self.pos), Pos::from(&self.pos), self.file.clone()));
                        self.consume();
                    }
                    
                    
                }
                ',' => {
                    tokens.push(Token::new(TokenType::Comma, Range::from(Pos::from(&self.pos)), Pos::from(&self.pos), Pos::from(&self.pos), self.file.clone()));
                    self.consume();
                }
                '.' => {
                    tokens.push(Token::new(TokenType::Dot, Range::from(Pos::from(&self.pos)), Pos::from(&self.pos), Pos::from(&self.pos), self.file.clone()));
                    self.consume();
                }
                '|' => {
                    tokens.push(Token::new(TokenType::Pipe, Range::from(Pos::from(&self.pos)), Pos::from(&self.pos), Pos::from(&self.pos), self.file.clone()));
                    self.consume();
                }
                '&' => {
                    tokens.push(Token::new(TokenType::Ampersand, Range::from(Pos::from(&self.pos)), Pos::from(&self.pos), Pos::from(&self.pos), self.file.clone()));
                    self.consume();
                }
                '^' => {
                    tokens.push(Token::new(TokenType::Caret, Range::from(Pos::from(&self.pos)), Pos::from(&self.pos), Pos::from(&self.pos), self.file.clone()));
                    self.consume();
                }
                '#' => {
                    tokens.push(Token::new(TokenType::Hash, Range::from(Pos::from(&self.pos)), Pos::from(&self.pos), Pos::from(&self.pos), self.file.clone()));
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
