#[derive(PartialEq, Eq, PartialOrd, Ord, Debug)]
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
    Dot,
    Max
}