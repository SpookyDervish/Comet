use miette::{Diagnostic, NamedSource, SourceSpan};
use thiserror::Error;

use crate::token::TokenType;

#[derive(Diagnostic, Debug, Error)]
#[error("SyntaxError")]
pub struct SyntaxError {
    #[source_code]
    pub src: NamedSource<String>,

    #[label("{text}")]
    pub span: SourceSpan,

    pub text: String
}

#[derive(Diagnostic, Debug, Error)]
#[error("NotAFunction")]
pub struct NotAFunction {
    #[source_code]
    pub src: NamedSource<String>,

    #[label("'{func_name}' is not a callable function")]
    pub span: SourceSpan,

    pub func_name: String
}

#[derive(Diagnostic, Debug, Error)]
#[error("NotAModule")]
pub struct NotAModule {
    #[source_code]
    pub src: NamedSource<String>,

    #[label("'{module_name}' is not a module")]
    pub span: SourceSpan,

    pub module_name: String
}

#[derive(Diagnostic, Debug, Error)]
#[error("UnkownType")]
pub struct UnkownType {
    #[source_code]
    pub src: NamedSource<String>,

    #[label("Unkown type '{type_}'")]
    pub span: SourceSpan,

    pub type_: String
}

#[derive(Diagnostic, Debug, Error)]
#[error("CompilerBug")]
pub struct CompilerBug {
    #[source_code]
    pub src: NamedSource<String>,

    #[label("{text}")]
    pub span: SourceSpan,

    pub text: String
}

#[derive(Diagnostic, Debug, Error)]
#[error("TypeMismatch")]
pub struct TypeMismatch {
    #[source_code]
    pub src: NamedSource<String>,

    #[label("Type '{invalid}' is not of type '{expected}'")]
    pub span: SourceSpan,

    pub invalid: String,
    pub expected: String,

    #[label("Type defined here")]
    pub type_def: Option<SourceSpan>
}

#[derive(Diagnostic, Debug, Error)]
#[error("UnkownField")]
pub struct UnkownField {
    #[source_code]
    pub src: NamedSource<String>,

    #[label("No field with the name '{field}' exists in the struct '{struct_name}'")]
    pub span: SourceSpan,

    pub field: String,
    pub struct_name: String
}

#[derive(Diagnostic, Debug, Error)]
#[error("UnkownUnionItem")]
pub struct UnkownUnionItem {
    #[source_code]
    pub src: NamedSource<String>,

    #[label("No item with the name '{item}' exists in the union '{union}'")]
    pub span: SourceSpan,

    pub item: String,
    pub union: String
}

#[derive(Diagnostic, Debug, Error)]
#[error("InvalidOperator")]
pub struct InvalidOperator {
    #[source_code]
    pub src: NamedSource<String>,

    #[label("Can't use operator '{op:?}' {value}")]
    pub span: SourceSpan,

    pub op: TokenType,
    pub value: String
}

#[derive(Diagnostic, Debug, Error)]
#[error("InvalidLValue")]
pub struct InvalidLValue {
    #[source_code]
    pub src: NamedSource<String>,

    #[label("{value} can't be an l-value")]
    pub span: SourceSpan,

    pub value: String
}

#[derive(Diagnostic, Debug, Error)]
#[error("ImmutableReassignment")]
pub struct ImmutableReassignment {
    #[source_code]
    pub src: NamedSource<String>,

    #[label("{var_name} is immutable and can't be reassigned")]
    pub span: SourceSpan,

    pub var_name: String
}

#[derive(Diagnostic, Debug, Error)]
#[error("InvalidCast")]
pub struct InvalidCast {
    #[source_code]
    pub src: NamedSource<String>,

    #[label("Cannot convert type '{old_type}' to type '{new_type}'")]
    pub span: SourceSpan,

    pub old_type: String,
    pub new_type: String
}

#[derive(Diagnostic, Debug, Error)]
#[error("UndefinedVariable")]
pub struct UndefinedVariable {
    #[source_code]
    pub src: NamedSource<String>,

    #[label("Use of undefined variable '{var}'")]
    pub span: SourceSpan,

    pub var: String
}

#[derive(Diagnostic, Debug, Error)]
#[error("InvalidCompilerDirective")]
pub struct InvalidCompilerDirective {
    #[source_code]
    pub src: NamedSource<String>,

    #[label("Invalid compiler directive '{directive}'")]
    pub span: SourceSpan,

    pub directive: String
}

#[derive(Diagnostic, Debug, Error)]
#[error("UnkownMethod")]
pub struct UnkownMethod {
    #[source_code]
    pub src: NamedSource<String>,

    #[label("Unkown method '{method}' in struct '{struct_name}'")]
    pub span: SourceSpan,

    pub method: String,
    pub struct_name: String
}

#[derive(Diagnostic, Debug, Error)]
#[error("InvalidVariableType")]
pub struct InvalidVariableType {
    #[source_code]
    pub src: NamedSource<String>,

    #[label("Variable cannot be of type '{type_name}'")]
    pub span: SourceSpan,

    pub type_name: String
}

#[derive(Diagnostic, Debug, Error)]
#[error("TypeAnnotationNeeded")]
pub struct TypeAnnotationNeeded {
    #[source_code]
    pub src: NamedSource<String>,

    #[label("Type annotation needed for variable '{var_name}'")]
    pub span: SourceSpan,

    pub var_name: String
}

#[derive(Diagnostic, Debug, Error)]
#[error("EmptyArrayLiteral")]
pub struct EmptyArrayLiteral {
    #[source_code]
    pub src: NamedSource<String>,

    #[label("Cannot have an empty array literal")]
    pub span: SourceSpan,
}

#[derive(Diagnostic, Debug, Error)]
#[error("NotImplemented")]
pub struct NotImplemented {
    #[source_code]
    pub src: NamedSource<String>,

    #[label("{text} is not implemented")]
    pub span: SourceSpan,

    pub text: String
}

#[derive(Diagnostic, Debug, Error)]
#[error("WrongNumberOfGenerics")]
pub struct WrongNumberOfGenerics {
    #[source_code]
    pub src: NamedSource<String>,

    #[label("Wrong number of generic types were passed to type '{type_name}'. Expected {expect}, got {got}")]
    pub span: SourceSpan,

    pub type_name: String,
    pub expect: usize,
    pub got: usize
}

#[derive(Diagnostic, Debug, Error)]
#[error("UnkownTrait")]
pub struct UnkownTrait {
    #[source_code]
    pub src: NamedSource<String>,

    #[label("Unkown trait '{trait_name}'")]
    pub span: SourceSpan,

    pub trait_name: String
}

#[derive(Diagnostic, Debug, Error)]
#[error("TraitNotDeclared")]
pub struct TraitNotDeclared {
    #[source_code]
    pub src: NamedSource<String>,

    #[label("The trait '{trait_name}' was no declared for the struct '{struct_name}'")]
    pub span: SourceSpan,

    pub trait_name: String,
    pub struct_name: String
}

#[derive(Diagnostic, Debug, Error)]
#[error("TraitAlreadyImplemented")]
pub struct TraitAlreadyImplemented {
    #[source_code]
    pub src: NamedSource<String>,

    #[label("The trait '{trait_name}' was is already implemented for the struct '{struct_name}'")]
    pub span: SourceSpan,

    pub trait_name: String,
    pub struct_name: String
}

#[derive(Diagnostic, Debug, Error)]
#[error("MissingTraitMethod")]
pub struct MissingTraitMethod {
    #[source_code]
    pub src: NamedSource<String>,

    #[label("The struct '{struct_name}' has trait '{trait_name}' but is missing the method '{method}'")]
    pub span: SourceSpan,

    pub trait_name: String,
    pub struct_name: String,
    pub method: String
}

#[derive(Diagnostic, Debug, Error)]
#[error("TraitSignatureMismatch")]
pub struct TraitSignatureMismatch {
    #[source_code]
    pub src: NamedSource<String>,

    #[label("The struct '{struct_name}' has trait '{trait_name}' but method '{method}' has the wrong signature. Expected '{expected}' but got '{got}'")]
    pub span: SourceSpan,

    pub trait_name: String,
    pub struct_name: String,
    pub method: String,

    pub expected: String,
    pub got: String
}