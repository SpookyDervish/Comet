#[derive(PartialEq, Eq, PartialOrd, Ord, Debug)]
pub enum PrecedenceType {
    Lowest,
    Or,
    And,
    Equals,
    LessGreater,
    Sum,
    Product,
    Prefix,
    Call,
    Index,
    Dot
}