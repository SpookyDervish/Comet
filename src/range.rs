use crate::position::Position;

#[derive(Debug)]
pub struct Range {
    start_line: usize,
    end_line: usize,

    start_column: usize,
    end_column: usize
}

impl Range {
    pub fn new(start_line: usize, end_line: usize, start_column: usize, end_column: usize) -> Self {
        Range {
            start_line: start_line, end_line: end_line,
            start_column: start_column, end_column: end_column
        }
    }

    pub fn from(pos: &Position) -> Self {
        Range { 
            start_line: pos.ln(), end_line: pos.ln(),
            start_column: pos.col(), end_column: pos.col()
        }
    }

    pub fn start(pos: &Position) -> Self {
        Range {
            start_line: pos.ln(), end_line: 0,
            start_column: pos.col(), end_column: 0
        }
    }

    pub fn end(&mut self, pos: &Position) {
        self.end_line = pos.ln();
        self.end_column = pos.col();
    }
}