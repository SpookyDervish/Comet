use miette::SourceSpan;
use std::collections::HashMap;

use crate::ast::ASTNode;

#[derive(Debug, Clone)]
pub struct CometTraitMethod {
    pub name: String,

    pub type_node: ASTNode,
    pub default_body: Option<ASTNode>
}

#[derive(Debug, Clone)]
pub struct CometTrait {
    pub name: String,
    pub methods: Vec<CometTraitMethod>,
    pub generics: Vec<String>,
    cached_methods: HashMap<String, usize>,
    definition_span: Option<SourceSpan>
}

/// Tracks whether a `struct X has Trait` obligation has been discharged.
///
/// Keyed by `(struct_name, trait_name)` in `Compiler::trait_impls`.
#[derive(Debug, Clone)]
pub struct TraitImplStatus {
    /// Set to true once an `imp X has Trait` block has supplied every method
    /// declared by the trait.
    pub satisfied: bool,

    /// Span of the `has Trait` clause in the struct declaration, used to point
    /// at it when the obligation is never discharged.
    pub declaration_span: SourceSpan
}

impl CometTrait {
    pub fn new(name: String, methods: Vec<CometTraitMethod>, generics: Vec<String>, definition_span: Option<SourceSpan>) -> Self {
        let mut cached_methods = HashMap::new();

        for (idx, method) in methods.iter().enumerate() {
            cached_methods.insert(method.name.clone(), idx);
        }
        
        CometTrait {
            name: name,
            generics: generics,
            methods: methods,
            cached_methods: cached_methods,
            definition_span: definition_span
        }
    }

    pub fn source_span(&self) -> SourceSpan {
        self.definition_span.unwrap().clone()
    }

    pub fn get_method(&self, name: &str) -> Option<&CometTraitMethod> {
        self.cached_methods.get(name).map(|i| &self.methods[*i])
    }
}