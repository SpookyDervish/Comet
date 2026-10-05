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
// Owned position and shared source file for lifetime-free tokens/ast
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Pos {
    index: usize,
    line: usize,
    column: usize,
}

impl Pos {
    pub fn new(index: usize, line: usize, column: usize) -> Self {
        Pos { index, line, column }
    }

    pub fn idx(&self) -> usize { self.index }
    pub fn ln(&self) -> usize { self.line }
    pub fn col(&self) -> usize { self.column }
}

impl From<&Position<'_>> for Pos {
    fn from(pos: &Position<'_>) -> Self {
        Pos { index: pos.idx(), line: pos.ln(), column: pos.col() }
    }
}

use std::rc::Rc;

#[derive(Debug, Clone)]
pub struct SourceFile {
    name: String,
    text: String,
}

impl SourceFile {
    pub fn new(name: String, text: String) -> Self {
        SourceFile { name, text }
    }

    pub fn shared(name: String, text: String) -> Rc<SourceFile> {
        Rc::new(SourceFile::new(name, text))
    }

    pub fn name(&self) -> &str { &self.name }
    pub fn text(&self) -> &str { &self.text }
    pub fn named_source(&self) -> miette::NamedSource<String> {
        miette::NamedSource::new(self.name.clone(), self.text.clone())
    }
}
