use crate::position::Position;

#[derive(Debug, Clone)]
pub struct Range {
    start_line: usize,
    end_line: usize,

    start_column: usize,
    end_column: usize,

    start_pos: usize,
    end_pos: usize
}

impl Range {
    pub fn new(start_line: usize, end_line: usize, start_column: usize, end_column: usize, start_pos: usize, end_pos: usize) -> Self {
        Range {
            start_line: start_line, end_line: end_line,
            start_column: start_column, end_column: end_column,
            start_pos: start_pos, end_pos: end_pos
        }
    }

    pub fn start_pos(&self) -> usize {
        self.start_pos
    }
    pub fn end_pos(&self) -> usize {
        self.end_pos
    }

    pub fn from(pos: &Position) -> Self {
        Range { 
            start_line: pos.ln(), end_line: pos.ln(),
            start_column: pos.col(), end_column: pos.col(),
            start_pos: pos.idx(), end_pos: pos.idx()
        }
    }

    pub fn start(pos: &Position) -> Self {
        Range {
            start_line: pos.ln(), end_line: 0,
            start_column: pos.col(), end_column: 0,
            start_pos: pos.idx(), end_pos: 0
        }
    }

    pub fn end(&mut self, pos: &Position) {
        self.end_line = pos.ln();
        self.end_column = pos.col();
        self.end_pos = pos.idx();
    }
}