use std::ops::Add;

#[derive(Debug, Clone)]
pub struct Position <'file> {
    index: usize,
    line: usize,
    column: usize,
    file_name: &'file str,
    file_text: &'file str
}

impl <'file> Position <'file> {
    pub fn new(file_name: &'file str, file_text: &'file str) -> Self {
        Position { index: 0, line: 0, column: 0, file_name: file_name, file_text: file_text }
    }

    pub fn advance(&mut self) {
        if self.file_text.chars().nth(self.index).unwrap() == '\n' {
            self.line += 1;
            self.column = 1;
        }

        self.index += 1;
        self.column += 1;
    }

    // getters
    pub fn idx(&self) -> usize {
        self.index
    }

    pub fn ln(&self) -> usize {
        self.line
    }

    pub fn col(&self) -> usize {
        self.column
    }

    pub fn file_name(&self) -> &'file str {
        self.file_name
    }

    pub fn source(&self) -> &'file str {
        self.file_text
    }
}