#[derive(PartialEq, Eq, PartialOrd, Ord)]
pub enum PrecedenceType {
    Lowest,
    Equals,
    LessGreater,
    Sum,
    Product,
    Exponent,
    Prefix,
    Call,
    Index,
    Max
}