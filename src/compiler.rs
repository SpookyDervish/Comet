use cranelift_codegen::ir::condcodes::{FloatCC, IntCC};
use cranelift_codegen::ir::immediates::Imm64;
use cranelift_codegen::{ir::AbiParam, settings};
use cranelift_codegen::ir::{self, Block, InstBuilder, InstBuilderBase, MemFlagsData, StackSlotData, StackSlotKind, types};
use cranelift_frontend::{FunctionBuilder, FunctionBuilderContext, Variable};
use cranelift_module::{DataDescription, FuncId, Linkage, Module, default_libcall_names};
use cranelift_object::{ObjectBuilder, ObjectModule};
use miette::{NamedSource, IntoDiagnostic};
use std::collections::{HashMap, HashSet};
use std::iter::zip;
use std::fs;
use itertools::Itertools;

use crate::ast::ASTType;
use crate::comet_error::{CompilerBug, EmptyArrayLiteral, ImmutableReassignment, InvalidCompilerDirective, InvalidLValue, InvalidOperator, InvalidVariableType, NotAFunction, NotAModule, SyntaxError, TypeAnnotationNeeded, TypeMismatch, UndefinedVariable, UnkownField, UnkownMethod, UnkownType, UnkownUnionItem, WrongNumberOfGenerics};
use crate::comet_struct::{CometStruct, CometStructField};
use crate::comet_type::{CometFunction, CometMethod, CometTypeKind, FunctionOwner};
use crate::comet_union::{CometUnion, CometUnionItem};
use crate::{lexer, parser, scope};
use crate::scope::CometVarType::Local;
use crate::scope::{CometVarType, CometVariable, ScopeFrame};
use crate::{ast::{ASTNode, ASTNodeType, MatchPattern}, comet_type::CometType, token::TokenType};

pub struct Compiler <'a> {
    module: ObjectModule,

    scopes: Vec<ScopeFrame<'a>>,
    methods: HashMap<(String, String), CometMethod>,
    resolved_generics: HashMap<String, CometType>,

    generic_impls: Vec<ASTNode<'a>>,

    current_function: Option<CometFunction>,

    file_name: &'a str,
    source: String,

    var_index: u32
}

impl <'a> Compiler <'a> {
    pub fn new(file_name: &'a str, source: String) -> miette::Result<Self, Box<dyn std::error::Error>> {
        let isa_builder = cranelift_native::builder()?;
        let isa = isa_builder.finish(settings::Flags::new(settings::builder()))?;

        let obj_builder = ObjectBuilder::new(
            isa,
            "comet_module",
            default_libcall_names()
        )?;

        let module = ObjectModule::new(obj_builder);

        let mut base_frame = ScopeFrame::new();

        // define all inbuilt types
        base_frame.types.insert("bool".to_string(), CometType::new(types::I8));
        base_frame.types.insert("i8".to_string(), CometType::new_int(types::I8, true));
        base_frame.types.insert("i16".to_string(), CometType::new_int(types::I16, true));
        base_frame.types.insert("i32".to_string(), CometType::new_int(types::I32, true));
        base_frame.types.insert("i64".to_string(), CometType::new_int(types::I64, true));
        base_frame.types.insert("isize".to_string(), CometType::new_int(module.isa().pointer_type(), true));
        base_frame.types.insert("u8".to_string(), CometType::new_int(types::I8, false));
        base_frame.types.insert("u16".to_string(), CometType::new_int(types::I16, false));
        base_frame.types.insert("u32".to_string(), CometType::new_int(types::I32, false));
        base_frame.types.insert("u64".to_string(), CometType::new_int(types::I64, false));
        base_frame.types.insert("usize".to_string(), CometType::new_int(module.isa().pointer_type(), false));
        base_frame.types.insert("f16".to_string(), CometType::new(types::F16));
        base_frame.types.insert("f32".to_string(), CometType::new(types::F32));
        base_frame.types.insert("f64".to_string(), CometType::new(types::F64));
        base_frame.types.insert("str".to_string(), CometType::new_ptr(CometType::new_int(types::I8, true), module.isa().pointer_type()));
        

        Ok(Compiler {
            module: module,
            scopes: vec!{base_frame},
            file_name: file_name,
            source: source,
            current_function: None,

            methods: HashMap::new(),
            resolved_generics: HashMap::new(),
            generic_impls: Vec::new(),

            var_index: 0
        })
    }

    pub fn end_module(self) -> miette::Result<Vec<u8>, cranelift_object::object::write::Error> {
        /* Compile finished program. */
        let module = self.module;

        let product = module.finish();
        let bytes = product.emit();

        return bytes;
    }

    // UTIL METHODS //
    fn named_source(&self) -> NamedSource<String> {
        NamedSource::new(self.file_name, self.source.clone())
    }

    fn get_variable(&self, name: &str) -> Option<&CometVariable<'a>> {
        self.scopes.iter().rev().find_map(|scope| scope.variables.get(name))
    }

    fn get_type(&self, name: &str) -> Option<&CometType> {
        self.scopes.iter().rev().find_map(|scope| scope.types.get(name))
    }

    fn get_method(&self, struct_name: String, method_name: String) -> Option<&CometMethod> {
        self.methods.get(&(struct_name, method_name))
    }

    fn get_generic(&self, name: &str) -> Option<&ASTNode<'a>> {
        self.scopes
            .iter()
            .rev()
            .find_map(|scope| scope.generics.get(name))
    }

    fn instantiate_generic_impls(
        &mut self,
        base_name: &str,
        concrete_types: &[CometType],
        concrete_struct: &CometStruct
    ) -> miette::Result<()> {

        let impls = self.generic_impls.clone();

        for impl_node in impls {
            let (struct_type_node, functions) = match impl_node.node_type() {
                ASTNodeType::ImplDefStatement { struct_type, functions } => (struct_type, functions),
                _ => unreachable!()
            };

            let target = match struct_type_node.node_type() {
                ASTNodeType::TypeLiteral { base_type, generic_types, .. } => (base_type, generic_types),
                _ => continue,
            };

            let type_name = match target.0 {
                ASTType::Identifier(name_node) => match name_node.as_ref().node_type() {
                    ASTNodeType::IdentifierLiteral(name) => name,
                    _ => continue,
                },
                _ => continue,
            };

            if type_name != base_name {
                continue;
            }

            
            let generic_names = match target.1.as_ref() {
                Some(generic_nodes) => generic_nodes
                    .iter()
                    .map(|n| match n.node_type() {
                        ASTNodeType::TypeLiteral {
                            base_type: ASTType::Identifier(name_node),
                            generic_types: None,
                        } => match name_node.node_type() {
                            ASTNodeType::IdentifierLiteral(name) => Ok(name.clone()),
                            _ => Err(()),
                        },
                        _ => Err(()),
                    })
                    .collect::<Result<Vec<_>, _>>(),
                None => continue,
            };

            let generic_names = match generic_names {
                Ok(names) => names,
                Err(()) => continue,
            };

            if generic_names.len() != concrete_types.len() {
                continue;
            }

            self.scopes.push(ScopeFrame::new());

            for (generic_name, concrete_type) in generic_names.iter().zip(concrete_types.iter()) {
                self.scopes.last_mut().unwrap().types.insert(
                    generic_name.clone(),
                    concrete_type.clone(),
                );
            }

            self.scopes.last_mut().unwrap().types.insert(
                "Self".to_string(),
                CometType::new_struct(concrete_struct.clone()),
            );

            for function in functions {
                let (name_node, args, return_type, body) = match function.node_type() {
                    ASTNodeType::FuncDefinitionStatement { name, args, return_type, body } => {
                        (name, args, return_type, body)
                    }
                    _ => unreachable!()
                };

                let method_name = match name_node.node_type() {
                    ASTNodeType::IdentifierLiteral(v) => v,
                    _ => unreachable!()
                };

                let func_id = self.compile_function(
                    method_name,
                    args,
                    return_type.as_ref().map(|r| r.as_ref()),
                    body,
                    FunctionOwner::Impl(concrete_struct.name().to_string()),
                )?;

                let mut arg_types = Vec::new();
                for func_arg in args {
                    let arg_type = match func_arg.node_type() {
                        ASTNodeType::FuncArgDefinition { type_, .. } => self.get_type_literal_type(type_)?,
                        _ => unreachable!()
                    };
                    arg_types.push(arg_type);
                }

                let mut compiled_return = CometType::new_void();

                if let Some(t) = return_type.as_ref() {
                    compiled_return = self.get_type_literal_type(t)?;
                }

                let function = CometFunction {
                    arg_types,
                    return_type: Box::new(compiled_return),
                };

                self.methods.insert(
                    (concrete_struct.name().to_string(), method_name.clone()),
                    CometMethod { function, func_id },
                );
            }

            self.scopes.pop();
        }

        Ok(())
    }

    fn get_generic_type_literal_type<'b>(
        &mut self,
        type_name_node: &ASTNode<'b>,
        generic_types: &[ASTNode<'b>]
    ) -> miette::Result<CometType> {
        println!("{:#?}", type_name_node.node_type());
        let type_name = match type_name_node.node_type() {
            ASTNodeType::QualifierNode { ident, .. } => {
                match ident.node_type() {
                    ASTNodeType::IdentifierLiteral(name) => name,
                    _ => unreachable!(),
                }
            },

            ASTNodeType::IdentifierLiteral(v) => v,

            _ => unreachable!(),
        };

        let mut type_template = self.get_generic(type_name)
            .ok_or_else(|| UnkownType {
                span: type_name_node.source_span(),
                src: self.named_source(),
                type_: type_name.clone(),
            })?
            .clone();

        let generic_names: Vec<String> = match type_template.node_type_mut() {
            ASTNodeType::StructDefinitionStatement { ident: _, fields: _, generics } => { 
                let generic_names = generics
                .as_ref()
                .unwrap()
                .iter()
                .map(|generic_node| match generic_node.node_type() {
                    ASTNodeType::IdentifierLiteral(v) => v.clone(),
                    _ => unreachable!()
                })
                .collect();

                *generics = None;

                generic_names
            },
            ASTNodeType::UnionDefinitionStatement { ident: _, fields: _, generics } => {
                let generic_names = generics
                .as_ref()
                .unwrap()
                .iter()
                .map(|generic_node| match generic_node.node_type() {
                    ASTNodeType::IdentifierLiteral(v) => v.clone(),
                    _ => unreachable!()
                })
                .collect();

                *generics = None;

                generic_names
            },
            _ => unreachable!()
        };

        self.scopes.push(ScopeFrame::new());

        if generic_types.len() != generic_names.len() {
            return Err(WrongNumberOfGenerics {
                expect: generic_names.len(),
                got: generic_types.len(),
                src: self.named_source(),
                span: type_name_node.source_span(),
                type_name: type_name.clone()
            }.into());
        }

        let resolved_generic_types: Vec<CometType> = generic_types
            .iter()
            .map(|t| self.get_type_literal_type(t))
            .collect::<miette::Result<Vec<_>>>()?;

        let existing_type_name = match type_template.node_type() {
            ASTNodeType::StructDefinitionStatement { .. } => {
                CometStruct::get_mangled_name(&type_name, &resolved_generic_types)
            }
            ASTNodeType::UnionDefinitionStatement { .. } => {
                CometUnion::get_mangled_name(&type_name, &resolved_generic_types)
            },
            _ => unreachable!()
        };

        let existing_generic = self.resolved_generics.get(&existing_type_name);
        if existing_generic.is_some() {
            let existing_generic = existing_generic.unwrap().clone();
            self.scopes.pop();
            self.scopes.last_mut().unwrap().types.insert(
                existing_type_name,
                existing_generic.clone()
            );
            return Ok(existing_generic);
        }

        for (i, generic_name) in generic_names.iter().enumerate() {
            self.scopes.last_mut().unwrap().types.insert(
                generic_name.clone(),
                resolved_generic_types[i].clone()
            );
        }

        
        match type_template.node_type() {
            ASTNodeType::StructDefinitionStatement { .. } => {
                let mut struct_result = self.visit_struct_def_statement(&type_template)?.unwrap();
                struct_result.mangle_name(&resolved_generic_types);

                self.instantiate_generic_impls(&type_name, &resolved_generic_types, &struct_result)?;

                let new_type = CometType::new_struct(struct_result.clone())
                    .with_definition_span(Some(type_name_node.source_span()));
                self.resolved_generics.insert(struct_result.name().to_string(), new_type.clone());

                self.scopes.pop();

                Ok(new_type)
            },
            ASTNodeType::UnionDefinitionStatement { .. } => {
                let union_result = self.visit_union_def_statement(&type_template, Some(&resolved_generic_types))?.unwrap();

                let union_name = union_result.name().to_string();
                let new_type = CometType::new_union(union_result)
                    .with_definition_span(Some(type_name_node.source_span()));
                self.resolved_generics.insert(union_name.clone(), new_type.clone());

                self.scopes.pop();

                self.scopes.last_mut().unwrap().types.insert(union_name, new_type.clone());

                

                Ok(new_type)
            },
            _ => unreachable!()
        }
        

        
    }


    fn qualified_name <'b> (&mut self, node: &ASTNode<'b>) -> miette::Result<String> {
        match node.node_type() {
            ASTNodeType::QualifierNode { ident: ident_node, generics } => {
                let ident = match ident_node.node_type() {
                    ASTNodeType::IdentifierLiteral(v) => v,
                    _ => unreachable!()
                };
                
                let generics_string = if (&generics).is_some() {
                    generics
                        .as_ref()
                        .unwrap()
                        .iter()
                        .map(|g| self.get_type_literal_type(g))
                        .collect::<miette::Result<Vec<CometType>>>()?
                        .iter()
                        .format("_")
                        .to_string()
                } else {
                    String::new()
                };

                Ok(format!("{}{}", ident, if generics_string.is_empty() { String::new() } else { format!("_{}", generics_string) }))
            },
            _ => unreachable!()
        }
    }

    fn get_type_literal_type<'b> (&mut self, node: &ASTNode<'b>) -> miette::Result<CometType> {
        /* Takes in a type literal node and returns the type it represents. */

        let (ast_type, generic_types) = match node.node_type() {
            ASTNodeType::TypeLiteral{ base_type, generic_types } => (base_type, generic_types),
            _ => unreachable!()
        };

        if generic_types.is_some() {
            return self.get_generic_type_literal_type(node, generic_types.as_ref().unwrap());
        }

        match ast_type {
            ASTType::Identifier(struct_name) => {
                if let ASTNodeType::QualifierNode { generics: Some(generic_types), .. } = struct_name.node_type() {
                    return self.get_generic_type_literal_type(struct_name, generic_types);
                }

                let ident = self.qualified_name(struct_name)?;

                self.get_type(&ident).cloned().ok_or(UnkownType {
                    span: struct_name.source_span(),
                    src: self.named_source(),
                    type_: ident
                }.into())
            },

            ASTType::Function { arg_types, return_type: return_type_node } => {
                let compiled_arg_types: miette::Result<Vec<CometType>> = arg_types
                                            .iter()
                                            .map(|t| self.get_type_literal_type(t))
                                            .collect();
                let compiled_arg_types = compiled_arg_types?;

                let return_type = return_type_node
                    .as_deref()
                    .map(|t| self.get_type_literal_type(t))
                    .transpose()?
                    .unwrap_or_else(|| CometType::new(types::INVALID));

                Ok(CometType::new_function(CometFunction {
                    arg_types: compiled_arg_types,
                    return_type: Box::new(return_type)
                }))
            },

            ASTType::Pointer(inner_node) => {
                let inner_type = self.get_type_literal_type(inner_node)?;
                Ok(CometType::new_ptr(inner_type, self.module.isa().pointer_type()))
            }

            ASTType::Array(inner_node, length_node) => {
                let inner_type = self.get_type_literal_type(inner_node)?;

                let length = match length_node.node_type() {
                    ASTNodeType::IntLiteral(n) => *n,
                    _ => unreachable!()
                };

                Ok(CometType::new_array(inner_type, length as u32, self.module.isa().pointer_type()))
            }

            ASTType::Qualified(names) => {
                if names.is_empty() {
                    return Err(SyntaxError {
                        span: node.source_span(),
                        src: self.named_source(),
                        text: "empty qualified type".to_string()
                    }.into());
                }

                // The first component is the type itself.
                //
                // Test<u64>::Hi
                // ^^^^^^^^^
                let first = &names[0];

                let mut current_type = match first.node_type() {
                    ASTNodeType::QualifierNode { generics, .. } => {
                        if let Some(generic_types) = generics {
                            // Resolve Test<u64>
                            self.get_generic_type_literal_type(
                                first,
                                generic_types
                            )?
                        } else {
                            // Resolve a non-generic Test
                            let ident = self.qualified_name(first)?;

                            self.get_type(&ident)
                                .cloned()
                                .ok_or_else(|| UnkownType {
                                    span: first.source_span(),
                                    src: self.named_source(),
                                    type_: ident
                                })?
                        }
                    }

                    _ => {
                        return Err(SyntaxError {
                            span: first.source_span(),
                            src: self.named_source(),
                            text: "expected type identifier".to_string()
                        }.into());
                    }
                };

                // Resolve the remaining qualified components.
                //
                // Test<u64>::Hi
                //            ^^
                for name in names.iter().skip(1) {
                    let ident = match name.node_type() {
                        ASTNodeType::QualifierNode { ident, generics } => {
                            if generics.is_some() {
                                return Err(SyntaxError {
                                    span: name.source_span(),
                                    src: self.named_source(),
                                    text: "generic arguments are not allowed here".to_string()
                                }.into());
                            }

                            match ident.node_type() {
                                ASTNodeType::IdentifierLiteral(v) => v,
                                _ => unreachable!()
                            }
                        }

                        ASTNodeType::IdentifierLiteral(v) => v,

                        _ => {
                            return Err(SyntaxError {
                                span: name.source_span(),
                                src: self.named_source(),
                                text: "expected identifier".to_string()
                            }.into());
                        }
                    };

                    current_type = match current_type.kind {
                        CometTypeKind::Union(union) => {
                            let item = union.get_item(ident)
                                .ok_or_else(|| UnkownUnionItem {
                                    item: ident.clone(),
                                    span: name.source_span(),
                                    src: self.named_source(),
                                    union: union.name().clone()
                                })?;

                            CometType::new_variant(item.clone())
                        }

                        _ => {
                            return Err(InvalidOperator {
                                op: TokenType::ColonColon,
                                span: name.source_span(),
                                src: self.named_source(),
                                value: String::from("on non-union type")
                            }.into());
                        }
                    };
                }

                Ok(current_type)
            },

            ASTType::ModuleQualified { path, type_name: type_name_node } => {

                let mut last_scope = &self.scopes[0];

                // loop over the path
                for ident in path {
                    let name = match ident.node_type() {
                        ASTNodeType::IdentifierLiteral(v) => v,
                        _ => unreachable!()
                    };

                    let var = last_scope.variables.get(name);
                    if var.is_none() {
                        return Err(UndefinedVariable {
                            var: name.clone(),
                            span: ident.source_span(),
                            src: self.named_source(),
                        }.into());
                    }

                    let module_scope = match &var.unwrap().var_type {
                        CometVarType::Module(s) => s,
                        _ => {
                            return Err(NotAModule {
                                src: self.named_source(),
                                span: ident.source_span(),
                                module_name: name.clone()
                            }.into()); 
                        }
                    };

                    last_scope = module_scope;
                    
                }

                // get the final field
                let type_name = match type_name_node.node_type() {
                    ASTNodeType::IdentifierLiteral(v) => v,
                    _ => unreachable!()
                };

                let type_var = last_scope.types.get(type_name);
                if type_var.is_none() {
                    return Err(UndefinedVariable {
                        span: type_name_node.source_span(),
                        src: self.named_source(),
                        var: type_name.clone()
                    }.into());
                }
                
                Ok(type_var.unwrap().clone())
            }
        }
        
    }

    fn rank_type(&self, type_: &CometType) -> u8 {
        match type_.cranelift_type {
            types::INVALID => 0,
            types::I8 => 1,
            types::I16 => 2,
            types::I32 => 3,
            types::I64 => 4,
            types::F16 => 5,
            types::F32 => 6,
            types::F64 => 7,
            _ => 0
        }
    }

    fn type_definition_span_for_error(invalid_type: Option<&CometType>, _expected_type: Option<&CometType>) -> Option<miette::SourceSpan> {
        invalid_type.and_then(|t| t.definition_span())
    }

    fn unify_types(&self, a: &'a CometType, b: &'a CometType) -> &CometType {
        if self.rank_type(a) >= 4 || self.rank_type(b) >= 4 {
            return if self.rank_type(a) > self.rank_type(b) {
                a
            } else {
                b
            };
        }

        if !a.is_int() || !b.is_int() {
            return if self.rank_type(a) > self.rank_type(b) {
                a
            } else {
                b
            };
        }

        let a_bits = a.cranelift_type.bits();
        let b_bits = b.cranelift_type.bits();

        let a_signed = a.is_signed();
        let b_signed = b.is_signed();

        if a_signed == b_signed {
            return if a_bits >= b_bits { a } else { b };
        }

        let (signed, unsigned) = if a_signed {
            (a, b)
        } else {
            (b, a)
        };

        let signed_bits = signed.cranelift_type.bits();
        let unsigned_bits = unsigned.cranelift_type.bits();

        if signed_bits > unsigned_bits {
            // e.g. i64 + u32 -> i64
            signed
        } else {
            // e.g. i32 + u32 -> u32
            //      i64 + u64 -> u64
            //      i32 + u64 -> u64
            unsigned
        }
    }

    fn visit_get_union_item(&mut self, _left: &ASTNode<'a>, right: &ASTNode<'a>, union: &CometUnion) -> miette::Result<CometType> {
        let right_ident = match right.node_type() {
            ASTNodeType::IdentifierLiteral(v) => v,
            _ => { return Err(SyntaxError {
                span: right.source_span(),
                src: self.named_source(),
                text: format!("Expected right token to be an identifier")
            }.into()); }
        };

        let item = union.get_item(right_ident);
        if item.is_none() {
            return Err(UnkownUnionItem {
                item: right_ident.clone(),
                span: right.source_span(),
                src: self.named_source(),
                union: union.name().clone()
            }.into());
        }

        Ok(CometType::new_variant(item.unwrap().clone()))
    }

    fn resolve_module_access_type(&mut self, node: &ASTNode<'a>) -> miette::Result<CometType> {
        let (left, right) = match node.node_type() {
            ASTNodeType::InfixExpression { left, op: _, right } => (left, right),
            _ => unreachable!()
        };
        
        let left_name = match left.node_type() {
            ASTNodeType::IdentifierLiteral(v) => v,
            _ => unreachable!()
        };

        let var_name = match right.node_type() {
            ASTNodeType::IdentifierLiteral(v) => v,
            _ => unreachable!()
        };

        let module_var = self.get_variable(&left_name.as_str()).ok_or(UndefinedVariable {
            span: node.source_span(),
            src: self.named_source(),
            var: left_name.clone()
        })?;

        let module = match &module_var.var_type {
            CometVarType::Module(s) => s,
            _ => unreachable!()
        };

        let var = module.variables.get(var_name);
        if var.is_none() {
            return Err(UnkownField {
                field: var_name.clone(),
                struct_name: left_name.clone(),
                span: right.source_span(),
                src: self.named_source()
            }.into());
        }
        let var = var.unwrap();

        println!("{:#?}", var.type_);

        return Ok(var.type_.clone());
    }

    fn resolve_type(&mut self, node: &ASTNode<'a>) -> miette::Result<CometType> {
        match node.node_type() {
            ASTNodeType::IntLiteral(_) => {
                Ok(CometType::new_int(types::I64, false))
            },

            ASTNodeType::FloatLiteral(_) => {
                Ok(CometType::new(types::F64))
            },

            ASTNodeType::StringLiteral(_) => {
                self.get_type("str").cloned().ok_or_else(|| CompilerBug {
                    span: node.source_span(),
                    src: self.named_source(),
                    text: String::from("failed to get internal \"str\" type, this is a bug!")
                }.into())
            },

            ASTNodeType::IdentifierLiteral(name) => {
                let var = self.get_variable(&name.as_str()).ok_or(UndefinedVariable {
                    span: node.source_span(),
                    src: self.named_source(),
                    var: name.clone()
                })?;

                Ok(var.type_.clone())
            },

            ASTNodeType::ArrayLiteral(elems) => {
                if elems.len() == 0 {
                    return Ok(CometType::new_unkown());
                }

                let base_elem_type = self.resolve_type(&elems[0])?;
                Ok(CometType::new_array(base_elem_type, elems.len() as u32, self.module.isa().pointer_type()))
            }

            ASTNodeType::IndexExpression { left, .. } => {
                let left_type = self.resolve_type(left)?;

                let elem_type = match left_type.kind {
                    CometTypeKind::Array { base_type, .. } => base_type,
                    _ => { return Err(TypeMismatch {
                        expected: "array".to_string(),
                        invalid: left_type.to_string(),
                        span: left.source_span(),
                        src: self.named_source(),
                        type_def: None
                    }.into()); }
                };

                Ok(*elem_type)
            },

            ASTNodeType::InfixExpression { left, op, right } => {
                let left_value = self.resolve_type(left)?;

                match &left_value.kind {
                    CometTypeKind::Union(u) => {
                        if op.token_type() == &TokenType::ColonColon {
                            return self.visit_get_union_item(left, right, &u);
                        }
                    },
                    _ => {}
                }

                match op.token_type() {
                    TokenType::Dot | TokenType::ColonColon => {
                        let struct_type = match left_value.kind {
                            CometTypeKind::Struct(comet_struct) => comet_struct,
                            CometTypeKind::Module => { return self.resolve_module_access_type(node); },
                            _ => { return Err(InvalidOperator {
                                    op: op.token_type().clone(),
                                    span: op.source_span(),
                                    src: self.named_source(),
                                    value: String::from("on non-struct")
                                }.into()); }
                        };

                        let field_name = match right.node_type() {
                            ASTNodeType::IdentifierLiteral(value) => value,
                            _ => return Err(SyntaxError {
                                src: self.named_source(),
                                text: format!("Expected identifier after '{:?}', got '{:?}' instead", op.token_type(), right.node_type()),
                                span: right.source_span()
                            }.into())
                        };

                        let method = self.get_method(struct_type.name().to_string(), field_name.to_string());
                        if method.is_some() { // getting method
                            let method = method.unwrap();
                            return Ok(CometType::new_function(method.function.clone()));
                        }

                        let field = struct_type.get_field(field_name).ok_or(UnkownField {
                            field: field_name.clone(),
                            struct_name: String::from(struct_type.name()),
                            span: right.source_span(),
                            src: self.named_source()
                        })?;

                        return Ok(field.field_type().clone());
                    },

                    TokenType::Divide => { return Ok(CometType::new(types::F64)); },

                    TokenType::Eq | TokenType::NotEq |
                    TokenType::Gt | TokenType::Lt |
                    TokenType::GtEq | TokenType::LtEq => { return Ok(CometType::new_int(types::I8, false)); },

                    _ => {}
                }

                let right_value = self.resolve_type(right)?;

                if op.token_type() == &TokenType::Or || op.token_type() == &TokenType::And {
                    return Ok(self.unify_types(&left_value, &right_value).clone());
                }

                return Ok(self.unify_types(&left_value, &right_value).clone());
            },

            ASTNodeType::PrefixExpression { op, right } => {
                let right_type = self.resolve_type(right)?;

                match op.token_type() {
                    TokenType::Ampersand => {
                        Ok(CometType::new_ptr(right_type, self.module.isa().pointer_type()))
                    },
                    TokenType::StarStar => {
                        let inner_type = match right_type.kind {
                            CometTypeKind::Pointer(t) => *t,
                            _ => { return Err(TypeMismatch {
                                src: self.named_source(),
                                span: right.source_span(),
                                invalid: right_type.to_string(),
                                expected: "pointer".to_string(),
                                type_def: None
                            }.into()); }
                        };

                        Ok(inner_type)
                    },

                    TokenType::Minus => {
                        let mut new_type = right_type.clone();                        
                        new_type.negate();
                        Ok(new_type)
                    }

                    TokenType::Tilde => {
                        if !right_type.is_int() {
                            return Err(InvalidOperator {
                                op: op.token_type().clone(),
                                span: op.source_span(),
                                src: self.named_source(),
                                value: "on non-integer".to_string()
                            }.into());
                        }

                        Ok(right_type)
                    }

                    TokenType::Not => {
                        self.get_type("bool").cloned().ok_or_else(|| CompilerBug {
                            span: node.source_span(),
                            src: self.named_source(),
                            text: String::from("failed to get internal \"bool\" type, this is a bug!")
                        }.into())
                    }

                    _ => Err(InvalidOperator {
                        op: op.token_type().clone(),
                        span: op.source_span(),
                        src: self.named_source(),
                        value: "as a prefix operator".to_string()
                    }.into())
                }
            },

            ASTNodeType::FuncCall { left, args: _ } => {
                let function_type = self.resolve_type(left)?;

                match function_type.kind {
                    CometTypeKind::Function(function) => Ok((*function.return_type).clone()),
                    _ => Err(NotAFunction {
                        span: left.source_span(),
                        src: self.named_source(),
                        func_name: match left.node_type() {
                            ASTNodeType::IdentifierLiteral(name) => name.clone(),
                            _ => String::from("<expression>"),
                        }
                    }.into())
                }
            },

            ASTNodeType::NewInstanceExpression { type_, fields: _ } => {
                let struct_type = self.get_type_literal_type(type_)?;

                match &struct_type.kind {
                    CometTypeKind::Variant(item) => {

                        self.get_type(item.union_name()).cloned().ok_or_else(|| CompilerBug {
                            span: type_.source_span(),
                            src: self.named_source(),
                            text: format!("union '{}' is missing from the type environment", item.union_name())
                        }.into())
                    },
                    _ => Ok(struct_type),
                }
            }

            _ => Err(CompilerBug {
                src: self.named_source(),
                span: node.source_span(),
                text: format!("Can't resolve type of '{:?}'", node.node_type())
            }.into())
        }
    }
    // END OF UTIL METHODS

    // VISIT METHODS //
    fn visit_program(&mut self, node: &ASTNode<'a>) -> miette::Result<()> {
        let nodes = match node.node_type() {
            ASTNodeType::Program(value) => value,
            _ => unreachable!()
        };

        for node in nodes {
            self.compile(node, None)?;
        }

        Ok(())
    }

    fn visit_block(&mut self, node: &ASTNode<'a>, builder: &mut FunctionBuilder) -> miette::Result<()> {
        let nodes = match node.node_type() {
            ASTNodeType::Block(value) => value,
            _ => unreachable!()
        };

        for node in nodes {
            self.compile(node, Some(builder))?;
        }

        Ok(())
    }

    fn visit_module_func_call(
        &mut self,
        receiver_node: &ASTNode<'a>,
        method_node: &ASTNode<'a>,
        args: &[ASTNode<'a>],
        builder: &mut FunctionBuilder
    ) -> miette::Result<ir::Value> {
        let var_name = match receiver_node.node_type() {
            ASTNodeType::IdentifierLiteral(v) => v,
            _ => unreachable!()
        };

        let module_var = self.get_variable(var_name).unwrap();

        let module_scope = match &module_var.var_type {
            CometVarType::Module(s) => s,
            _ => unreachable!()
        };

        let func_name = match method_node.node_type() {
            ASTNodeType::IdentifierLiteral(v) => v,
            _ => unreachable!()
        };

        let func_var = module_scope.variables.get(func_name);
        if func_var.is_none() {
            return Err(UnkownMethod {
                span: method_node.source_span(),
                src: self.named_source(),
                method: func_name.clone(),
                struct_name: var_name.clone()
            }.into());
        }
        let func_var = func_var.unwrap();

        let func = match &func_var.type_.kind {
            CometTypeKind::Function(f) => f,
            _ => unreachable!()
        };

        // build signature
        let mut sig = self.module.make_signature();

        for arg_type in &func.arg_types {
            sig.params.push(AbiParam::new(arg_type.cranelift_type));
        }

        if func.return_type.cranelift_type != types::INVALID {
            sig.returns.push(AbiParam::new(
                func.return_type.cranelift_type,
            ));
        }

        let func_id = self.module
                                    .declare_function(func_name, Linkage::Import, &sig)
                                    .unwrap();

        let func_ref = self
            .module
            .declare_func_in_func(func_id, builder.func);

        let mut compiled_args = vec![];

        for arg in args {
            compiled_args.push(self.visit_value(arg, builder)?);
        }

        let call_inst = builder.ins().call(func_ref, &compiled_args);
        let results = builder.inst_results(call_inst);

        if results.is_empty() {
            Ok(builder.ins().iconst(types::I64, 0))
        } else {
            Ok(results[0])
        }
    }

    fn visit_method_call(
        &mut self,
        receiver_node: &ASTNode<'a>,
        method_node: &ASTNode<'a>,
        op: &TokenType,
        args: &[ASTNode<'a>],
        builder: &mut FunctionBuilder
    ) -> miette::Result<ir::Value> {
        let receiver_type = self.resolve_type(receiver_node)?;

        let struct_name = match &receiver_type.kind {
            CometTypeKind::Struct(comet_struct) => comet_struct.name(),
            CometTypeKind::Module => { return self.visit_module_func_call(receiver_node, method_node, args, builder); },
            _ => {
                return Err(TypeMismatch {
                    expected: "struct".to_string(),
                    invalid: receiver_type.to_string(),
                    span: receiver_node.source_span(),
                    src: self.named_source(),
                    type_def: None
                }.into());
            }
        };

        let method_name = match method_node.node_type() {
            ASTNodeType::IdentifierLiteral(name) => name,
            _ => unreachable!(),
        };

        

        let receiver_value = self.visit_value(receiver_node, builder)?;

        let mut compiled_args = vec![];

        if op == &TokenType::Dot {
            compiled_args.push(receiver_value);
        }

        for arg in args {
            compiled_args.push(self.visit_value(arg, builder)?);
        }

        let method = self
            .get_method(struct_name.to_string(), method_name.clone())
            .ok_or(UnkownMethod {
                method: method_name.clone(),
                struct_name: struct_name.to_string(),
                span: method_node.source_span(),
                src: self.named_source()
            })?;

        // method.function.arg_types includes self as argument zero
        let mut sig = self.module.make_signature();

        for arg_type in &method.function.arg_types {
            sig.params.push(AbiParam::new(arg_type.cranelift_type));
        }

        if method.function.return_type.cranelift_type != types::INVALID {
            sig.returns.push(AbiParam::new(
                method.function.return_type.cranelift_type,
            ));
        }

        let method_ref = self
            .module
            .declare_func_in_func(method.func_id, builder.func);

        let call_inst = builder.ins().call(method_ref, &compiled_args);
        let results = builder.inst_results(call_inst);

        if results.is_empty() {
            Ok(builder.ins().iconst(types::I64, 0))
        } else {
            Ok(results[0])
        }
    }

    fn visit_variant_instance(&mut self, item: &CometUnionItem, fields: &Option<Vec<ASTNode<'a>>>, span: miette::SourceSpan, builder: &mut FunctionBuilder) -> miette::Result<ir::Value> {
        let discriminant_type = types::I32;
        let payload_offset = item.payload_offset(discriminant_type.bytes());
        let union_type = self.get_type(item.union_name()).cloned().ok_or_else(|| CompilerBug {
            span: span.clone(),
            src: self.named_source(),
            text: format!("union '{}' is missing from the type environment", item.union_name())
        })?;
        let union = match union_type.kind {
            CometTypeKind::Union(union) => union,
            _ => unreachable!()
        };
        let supplied_fields = fields.as_deref().unwrap_or(&[]);

        if supplied_fields.len() != item.field_names().len() {
            return Err(SyntaxError {
                span,
                src: self.named_source(),
                text: format!("variant '{}' expects {} payload fields, got {}", item.name(), item.field_names().len(), supplied_fields.len())
            }.into());
        }

        let stack_slot = builder.create_sized_stack_slot(StackSlotData::new(
            StackSlotKind::ExplicitSlot,
            union.storage_size(discriminant_type.bytes()),
            union.storage_align(discriminant_type.bytes()).max(1).trailing_zeros() as u8
        ));

        let stack_addr = builder.ins().stack_addr(
            self.module.isa().pointer_type(),
            stack_slot,
            0
        );

        let discriminant_value = builder.ins().iconst(discriminant_type, item.discriminant() as i64);
        builder.ins().store(
            MemFlagsData::new(),
            discriminant_value,
            stack_addr,
            0
        );

        let mut written_fields = HashMap::new();
        for field in supplied_fields {
            let (field_name_node, value_node) = match field.node_type() {
                ASTNodeType::StructField { ident, value } => (ident, value),
                _ => unreachable!()
            };

            let field_name = match field_name_node.node_type() {
                ASTNodeType::IdentifierLiteral(name) => name,
                _ => unreachable!()
            };

            if written_fields.insert(field_name.as_str(), ()).is_some() {
                return Err(SyntaxError {
                    span: field_name_node.source_span(),
                    src: self.named_source(),
                    text: format!("payload field '{}' is specified more than once", field_name)
                }.into());
            }

            let field_type = item.get_field(field_name).ok_or_else(|| UnkownField {
                field: field_name.clone(),
                struct_name: item.name().clone(),
                span: field_name_node.source_span(),
                src: self.named_source()
            })?;

            let value_type = self.resolve_type(value_node)?;
            let mut value = self.visit_value(value_node, builder)?;
            if value_type != *field_type || value_type.cranelift_type != field_type.cranelift_type {
                value = CometType::try_implicit_cast(
                    value_node,
                    value,
                    &value_type,
                    field_type,
                    Some(field_name_node),
                    builder,
                    self.named_source()
                )?;
            }
            let field_offset = item.field_offset(field_name).unwrap();
            builder.ins().store(
                MemFlagsData::new(),
                value,
                stack_addr,
                (payload_offset + field_offset) as i32
            );
        }

        for field_name in item.field_names() {
            if !written_fields.contains_key(field_name.as_str()) {
                return Err(SyntaxError {
                    span: span.clone(),
                    src: self.named_source(),
                    text: format!("missing payload field '{}' for variant '{}'", field_name, item.name())
                }.into());
            }
        }

        Ok(stack_addr)
    }

    fn visit_value(&mut self, node: &ASTNode<'a>, builder: &mut FunctionBuilder) -> miette::Result<ir::Value> {
        match node.node_type() {
            ASTNodeType::InfixExpression { left: _, op, right: _ } => {
                if op.token_type() == &TokenType::Dot {
                    self.visit_get_field(node, builder)
                } else {
                    self.visit_infix_expression(node, builder)
                }
            },

            ASTNodeType::PrefixExpression { op: _, right: _ } => self.visit_prefix_expression(node, builder),

            ASTNodeType::IntLiteral(num) => {
                Ok(builder.ins().iconst(types::I64, *num as i64))
            },

            ASTNodeType::FloatLiteral(num) => {
                Ok(builder.ins().f64const(*num as f64))
            },

            ASTNodeType::StringLiteral(str) => {
                let mut data_desc = DataDescription::new();
                data_desc.define(str.as_bytes().to_vec().into_boxed_slice());

                let string_name = format!("string_literal_{}", self.var_index);
                let data_id = self.module
                    .declare_data(&string_name, Linkage::Local, false, false).unwrap();

                self.module.define_data(data_id, &data_desc).unwrap();

                let local_data_id = self.module.declare_data_in_func(data_id, &mut builder.func);

                let pointer_type = self.module.isa().pointer_type();

                let ptr = builder.ins().symbol_value(pointer_type, local_data_id);

                Ok(ptr)
            },

            ASTNodeType::IdentifierLiteral(var_name) => {
                let comet_var: &CometVariable = self.get_variable(&var_name.as_str()).ok_or(UndefinedVariable {
                    span: node.source_span(),
                    src: self.named_source(),
                    var: var_name.clone()
                })?;

                if let Some(func_id) = comet_var.function_id {
                    let func_ref = self.module.declare_func_in_func(func_id, builder.func);
                    let pointer_type = self.module.isa().frontend_config().pointer_type();
                    return Ok(builder.ins().func_addr(pointer_type, func_ref));
                }

                match &comet_var.var_type {
                    CometVarType::Local(var) => Ok(builder.use_var(*var)),
                    CometVarType::FuncArg(index) => {
                        let params = builder.block_params(builder.current_block().unwrap());
                        Ok(params[*index])
                    },
                    CometVarType::External(func_id) => {
                        let func_ref = self.module.declare_func_in_func(*func_id, builder.func);
                        let pointer_type = self.module.isa().frontend_config().pointer_type();

                        Ok(builder.ins().func_addr(pointer_type, func_ref))
                    },
                    CometVarType::Module(_) => {
                        Ok(builder.ins().build_imm_const(types::INVALID, Imm64::new(0), false))
                    }
                }  
            },

            ASTNodeType::ArrayLiteral(elems) => {
                if elems.len() == 0 {
                    return Err(EmptyArrayLiteral {
                        src: self.named_source(),
                        span: node.source_span()
                    }.into());
                }

                let base_type = match self.resolve_type(node)?.kind {
                    CometTypeKind::Array { base_type, .. } => base_type,
                    _ => unreachable!()
                };

                let stack_slot = builder.create_sized_stack_slot(StackSlotData::new(
                    StackSlotKind::ExplicitSlot,
                    base_type.size() * elems.len() as u32,
                    base_type.align() as u8
                ));

                let stack_addr = builder.ins().stack_addr(self.module.isa().pointer_type(), stack_slot, 0);

                let elem_size = base_type.size() as i32;

                for (i, elem) in elems.iter().enumerate() {
                    let elem_value = self.visit_value(elem, builder)?;

                    builder.ins().store(
                        MemFlagsData::new(),
                        elem_value,
                        stack_addr,
                        (i as i32) * elem_size
                    );
                }

                Ok(stack_addr)
            },

            ASTNodeType::IndexExpression { left, index } => {
                let left_type = self.resolve_type(left)?;

                let elem_type = match left_type.kind {
                    CometTypeKind::Array { base_type, .. } => base_type,
                    _ => { return Err(TypeMismatch {
                        expected: "array".to_string(),
                        invalid: left_type.to_string(),
                        span: left.source_span(),
                        src: self.named_source(),
                        type_def: None
                    }.into()); }
                };

                let array_value = self.visit_value(left, builder)?;
                let index = self.visit_value(index, builder)?;

                let ptr_offset = builder.ins().imul_imm_s(index, elem_type.size() as i64);
                let ptr = builder.ins().iadd(array_value, ptr_offset);

                Ok(builder.ins().load(
                    elem_type.cranelift_type,
                    MemFlagsData::new(),
                    ptr,
                    0
                ))
            },

            ASTNodeType::FuncCall { left, args } => {
                // check if we're calling a method
                if let ASTNodeType::InfixExpression {
                    left,
                    op,
                    right
                } = left.node_type()
                {
                    if op.token_type() == &TokenType::Dot || op.token_type() == &TokenType::ColonColon {
                        return self.visit_method_call(
                            left,
                            right,
                            op.token_type(),
                            args,
                            builder
                        );
                    }
                }


                let func_ptr = self.visit_value(left, builder)?;
                let func_type = self.resolve_type(left)?;

                let comet_function = match func_type.kind {
                    CometTypeKind::Function(f) => f,
                    _ => { return Err(NotAFunction {
                        span: (**left).source_span(),
                        src: NamedSource::new(String::from(self.file_name), self.source.clone()),
                        func_name: String::from("<expression>"),
                    }.into()); }
                };

                let mut compiled_args: Vec<ir::Value> = vec![];
                for arg in args {
                    compiled_args.push(self.visit_value(arg, builder)?);
                }

                let mut sig = self.module.make_signature();
                for arg_type in &comet_function.arg_types {
                    sig.params.push(AbiParam::new(arg_type.cranelift_type));
                }
                if comet_function.return_type.cranelift_type != types::INVALID {
                    sig.returns.push(AbiParam::new(comet_function.return_type.cranelift_type));
                }
                
                let sig_ref = builder.import_signature(sig);
                let call_inst = builder.ins().call_indirect(sig_ref, func_ptr, &compiled_args);
                let results = builder.inst_results(call_inst);


                if results.is_empty() {
                    // Return a dummy value or handle void returns
                    Ok(builder.ins().iconst(types::I64, 0))
                } else {
                    Ok(results[0])
                }
                
            },

            ASTNodeType::NewInstanceExpression { type_: type_node, fields } => {
                let struct_type = self.get_type_literal_type(type_node)?;

                // we're creating an instance of an array
                match &struct_type.kind {
                    CometTypeKind::Array { base_type, size } => {
                        let stack_slot = builder.create_sized_stack_slot(StackSlotData::new(
                            StackSlotKind::ExplicitSlot,
                            base_type.size() * *size as u32,
                            base_type.align() as u8
                        ));

                        let stack_addr = builder.ins().stack_addr(self.module.isa().pointer_type(), stack_slot, 0);
                        return Ok(stack_addr);
                    }

                    CometTypeKind::Variant(i) => { return self.visit_variant_instance(i, fields, type_node.source_span(), builder); }

                    _ => {}
                }

                let comet_struct = match struct_type.kind {
                    CometTypeKind::Struct(value) => value,

                    _ => { return Err(TypeMismatch {
                        src: self.named_source(),
                        expected: String::from("struct"),
                        invalid: struct_type.to_string(),
                        span: type_node.source_span(),
                        type_def: None
                    }.into()); }
                };

                let struct_layout = comet_struct.get_layout();

                let stack_slot = builder.create_sized_stack_slot(StackSlotData::new( 
                    StackSlotKind::ExplicitSlot,
                    struct_layout.1,
                    8
                ));
                let addr = builder.ins().stack_addr(self.module.isa().pointer_type(), stack_slot, 0);

                let fields_layout = struct_layout.0;

                for field in fields.as_ref().unwrap() {
                    let (ident_node, value_node) = match field.node_type() {
                        ASTNodeType::StructField { ident, value } => (ident, value),
                        _ => unreachable!()
                    };

                    let field_name = match ident_node.node_type() {
                        ASTNodeType::IdentifierLiteral(value) => value,
                        _ => unreachable!()
                    };

                    let field_value = self.visit_value(&value_node, builder)?;

                    let field_index = comet_struct.get_field_index(&field_name);
                    if field_index.is_none() {
                        return Err(UnkownField {
                            field: field_name.clone(),
                            struct_name: String::from(comet_struct.name()),
                            span: ident_node.source_span(),
                            src: self.named_source()
                        }.into());
                    }
                    let field_index = field_index.unwrap();

                    let field_offset = fields_layout[*field_index];
                    builder.ins().store(MemFlagsData::new(), field_value, addr, field_offset as i32);
                }
                

                Ok(addr)
            },

            _ => Err(CompilerBug {
                src: self.named_source(),
                span: node.source_span(),
                text: format!("Cannot compile r-value '{:?}'", node.node_type())
            }.into())
        }
    }

    fn visit_l_value(&mut self, node: &ASTNode<'a>, builder: &mut FunctionBuilder) -> miette::Result<ir::Value> {
        match node.node_type() {
            ASTNodeType::IdentifierLiteral(_) => self.visit_value(node, builder),

            ASTNodeType::PrefixExpression { op, right } => {
                let right_type = self.resolve_type(right)?;
                
                match op.token_type() {
                    TokenType::StarStar => {
                        match right_type.kind {
                            CometTypeKind::Pointer(_) => {},
                            _ => { return Err(TypeMismatch {
                                src: self.named_source(),
                                span: right.source_span(),
                                invalid: right_type.to_string(),
                                expected: "pointer".to_string(),
                                type_def: None
                            }.into()); }
                        }

                        let right_value = self.visit_value(right, builder)?;
                        Ok(right_value)
                    }
                    _ => Err(InvalidOperator {
                        src: self.named_source(),
                        span: op.source_span(),
                        op: op.token_type().clone(),
                        value: "as a prefix operator in an l-value".to_string()
                    }.into())
                }
            },

            ASTNodeType::IndexExpression { left, index } => {
                let left_type = self.resolve_type(left)?;

                let elem_type = match left_type.kind {
                    CometTypeKind::Array { base_type, .. } => base_type,
                    _ => { return Err(TypeMismatch {
                        expected: "array".to_string(),
                        invalid: left_type.to_string(),
                        span: left.source_span(),
                        src: self.named_source(),
                        type_def: None
                    }.into()); }
                };

                let array_value = self.visit_value(left, builder)?;
                let index = self.visit_value(index, builder)?;

                let ptr_offset = builder.ins().imul_imm_s(index, elem_type.size() as i64);
                let ptr = builder.ins().iadd(array_value, ptr_offset);

                Ok(ptr)
            },

            ASTNodeType::InfixExpression { left, op, right } => {
                let left_type = self.resolve_type(left)?;
                let left = self.visit_l_value(left, builder)?;

                match op.token_type() {
                    TokenType::Dot => {
                        let comet_struct = match left_type.kind {
                            CometTypeKind::Struct(value) => value,
                            _ => {  return Err(InvalidOperator {
                                        op: op.token_type().clone(),
                                        span: op.source_span(),
                                        src: self.named_source(),
                                        value: String::from("on non-struct")
                                    }.into()); }
                        };

                        let field_name = match right.node_type() {
                            ASTNodeType::IdentifierLiteral(value) => value,
                            _ => { return Err(SyntaxError {
                                src: self.named_source(),
                                text: format!("Expected identifier after '{:?}', got '{:?}' instead", op.token_type(), right.node_type()),
                                span: right.source_span()
                            }.into()); }
                        };

                        let comet_struct_layout = comet_struct.get_layout();

                        let field = comet_struct.get_field(field_name);
                        if field.is_none() {
                            return Err(UnkownField {
                                field: field_name.clone(),
                                struct_name: String::from(comet_struct.name()),
                                span: right.source_span(),
                                src: self.named_source()
                            }.into());
                        }
                        let field_index = comet_struct.get_field_index(field_name).unwrap();

                        Ok(builder.ins().iadd_imm_s(left, comet_struct_layout.0[*field_index] as i64))
                    },


                    _ => Err(InvalidOperator {
                            op: op.token_type().clone(),
                            span: op.source_span(),
                            src: self.named_source(),
                            value: String::from("on l-value")
                        }.into())
                }
            },
            _ => Err(InvalidLValue {
                src: self.named_source(),
                span: node.source_span(),
                value: format!("{:?}", node.node_type())
            }.into())
        }
    }

    fn visit_logical_op_expr(&mut self, left: &ASTNode<'a>, op: &TokenType, right: &ASTNode<'a>, builder: &mut FunctionBuilder) -> miette::Result<ir::Value> {
        let left_type = self.resolve_type(left)?;
        let right_type = self.resolve_type(right)?;
        let unified_type = self.unify_types(&left_type, &right_type).clone();

        let end_block = builder.create_block();
        let result = builder.append_block_param(end_block, unified_type.cranelift_type);
        let short_circuit_block = builder.create_block();
        let right_block = builder.create_block();

        let left_value = self.visit_value(left, builder)?;
        let mut left_result = left_value;
        if left_type != unified_type {
            left_result = CometType::try_implicit_cast(
                left,
                left_result,
                &left_type,
                &unified_type,
                None,
                builder,
                self.named_source()
            )?;
        }

        if op == &TokenType::And {
            builder.ins().brif(left_value, right_block, &[], short_circuit_block, &[]);
            builder.seal_block(builder.current_block().unwrap());

            builder.switch_to_block(short_circuit_block);
            builder.ins().jump(end_block, &[ir::BlockArg::Value(left_result)]);
            builder.seal_block(short_circuit_block);

            builder.switch_to_block(right_block);
            let right_value = self.visit_value(right, builder)?;
            let right_result = if right_type != unified_type {
                CometType::try_implicit_cast(
                    right,
                    right_value,
                    &right_type,
                    &unified_type,
                    None,
                    builder,
                    self.named_source()
                )?
            } else {
                right_value
            };
            builder.ins().jump(end_block, &[ir::BlockArg::Value(right_result)]);
            builder.seal_block(right_block);
        } else {
            builder.ins().brif(left_value, short_circuit_block, &[], right_block, &[]);
            builder.seal_block(builder.current_block().unwrap());

            builder.switch_to_block(short_circuit_block);
            builder.ins().jump(end_block, &[ir::BlockArg::Value(left_result)]);
            builder.seal_block(short_circuit_block);

            builder.switch_to_block(right_block);
            let right_value = self.visit_value(right, builder)?;
            let right_result = if right_type != unified_type {
                CometType::try_implicit_cast(
                    right,
                    right_value,
                    &right_type,
                    &unified_type,
                    None,
                    builder,
                    self.named_source()
                )?
            } else {
                right_value
            };
            builder.ins().jump(end_block, &[ir::BlockArg::Value(right_result)]);
            builder.seal_block(right_block);
        }

        builder.switch_to_block(end_block);
        builder.ensure_inserted_block();

        Ok(result)
    }

    fn visit_infix_expression(&mut self, node: &ASTNode<'a>, builder: &mut FunctionBuilder) -> miette::Result<ir::Value> {
        let (left, op, right) = match node.node_type() {
            ASTNodeType::InfixExpression { left, op, right } => (left, op, right),
            _ => unreachable!()
        };
        

        if op.token_type() == &TokenType::Or || op.token_type() == &TokenType::And {
            return self.visit_logical_op_expr(left, op.token_type(), right, builder);
        }

        let mut left_side = self.visit_value(left, builder)?;
        let mut right_side = self.visit_value(right, builder)?;

        let left_type = self.resolve_type(left)?;
        let right_type = self.resolve_type(right)?;
        let unified_type = self.unify_types(&left_type, &right_type);

        let is_signed = unified_type.is_signed();
        let is_int = unified_type.is_int();

        if &left_type != unified_type {
            left_side = CometType::try_implicit_cast(
                left,
                left_side,
                &left_type,
                unified_type,
                None,
                builder,
                self.named_source()
            )?;
        }
        if &right_type != unified_type {
            right_side = CometType::try_implicit_cast(
                right,
                right_side,
                &right_type,
                unified_type,
                None,
                builder,
                self.named_source()
            )?;
        }

        let out: ir::Value;
        match op.token_type() {
            TokenType::Plus => {

                out = if is_int {
                    builder.ins().iadd(left_side, right_side)
                } else {
                    builder.ins().fadd(left_side, right_side)
                }
            },
            TokenType::Minus => {
                out = if is_int {
                    builder.ins().isub(left_side, right_side)
                } else {
                    builder.ins().fsub(left_side, right_side)
                }
            },
            TokenType::Times => {
                out = if is_int {
                    builder.ins().imul(left_side, right_side)
                } else {
                    builder.ins().fmul(left_side, right_side)
                }
            },
            TokenType::Divide => {
                out = if is_int {
                    if is_signed {
                        builder.ins().sdiv(left_side, right_side)
                    } else {
                        builder.ins().udiv(left_side, right_side)
                    }
                } else {
                    builder.ins().fdiv(left_side, right_side)
                }
            },
            TokenType::Modulo => {
                if !is_int {
                    return Err(InvalidOperator {
                        op: op.token_type().clone(),
                        span: op.source_span(),
                        src: self.named_source(),
                        value: "on non-integer".to_string()
                    }.into());
                }

                out = if is_signed {
                    builder.ins().srem(left_side, right_side)
                } else {
                    builder.ins().urem(left_side, right_side)
                }
            },
            TokenType::Ampersand => {
                if !is_int {
                    return Err(InvalidOperator {
                        op: op.token_type().clone(),
                        span: op.source_span(),
                        src: self.named_source(),
                        value: "on non-integer".to_string()
                    }.into());
                }

                out = builder.ins().band(left_side, right_side);
            }
            TokenType::Pipe => {
                if !is_int {
                    return Err(InvalidOperator {
                        op: op.token_type().clone(),
                        span: op.source_span(),
                        src: self.named_source(),
                        value: "on non-integer".to_string()
                    }.into());
                }

                out = builder.ins().bor(left_side, right_side);
            }
            TokenType::Caret => {
                if !is_int {
                    return Err(InvalidOperator {
                        op: op.token_type().clone(),
                        span: op.source_span(),
                        src: self.named_source(),
                        value: "on non-integer".to_string()
                    }.into());
                }

                out = builder.ins().bxor(left_side, right_side);
            }
            TokenType::EqEq => {
                out = if is_int {
                    builder.ins().icmp(IntCC::Equal, left_side, right_side)
                } else {
                    builder.ins().fcmp(FloatCC::Equal, left_side, right_side)
                }
            },
            TokenType::NotEq => {
                out = if is_int {
                    builder.ins().icmp(IntCC::NotEqual, left_side, right_side)
                } else {
                    builder.ins().fcmp(FloatCC::NotEqual, left_side, right_side)
                }
            },
            TokenType::Lt => {
                out = if is_int {
                    builder.ins().icmp(
                        if is_signed {IntCC::SignedLessThan} else {IntCC::UnsignedLessThan},
                        left_side,
                        right_side
                    )
                } else {
                    builder.ins().fcmp(
                        FloatCC::LessThan,
                        left_side,
                        right_side
                    )
                }
            },
            TokenType::Gt => {
                out = if is_int {
                    builder.ins().icmp(
                        if is_signed {IntCC::SignedGreaterThan} else {IntCC::UnsignedGreaterThan},
                        left_side,
                        right_side
                    )
                } else {
                    builder.ins().fcmp(
                        FloatCC::GreaterThan,
                        left_side,
                        right_side
                    )
                }
            },
            TokenType::LtEq => {
                out = if is_int {
                    builder.ins().icmp(
                        if is_signed {IntCC::SignedLessThanOrEqual} else {IntCC::UnsignedLessThanOrEqual},
                        left_side,
                        right_side
                    )
                } else {
                    builder.ins().fcmp(
                        FloatCC::LessThanOrEqual,
                        left_side,
                        right_side
                    )
                }
            },
            TokenType::GtEq => {
                out = if is_int {
                    builder.ins().icmp(
                        if is_signed {IntCC::SignedGreaterThanOrEqual} else {IntCC::UnsignedGreaterThanOrEqual},
                        left_side,
                        right_side
                    )
                } else {
                    builder.ins().fcmp(
                        FloatCC::GreaterThanOrEqual,
                        left_side,
                        right_side
                    )
                }
            },

            _ => {
                return Err(InvalidOperator {
                    op: op.token_type().clone(),
                    span: op.source_span(),
                    src: self.named_source(),
                    value: String::from("for infix expression")
                }.into());
            }
        }

        Ok(out)
    }

    fn visit_get_field(&mut self, node: &ASTNode<'a>, builder: &mut FunctionBuilder) -> miette::Result<ir::Value> {
        let (left_node, op, right_node) = match node.node_type() {
            ASTNodeType::InfixExpression { left, op, right } => (left, op, right),
            _ => unreachable!()
        };

        let left = self.visit_value(left_node, builder)?;
        let struct_type = self.resolve_type(left_node)?;
        let comet_struct = match struct_type.kind {
            CometTypeKind::Struct(value) => value,
            _ => {  return Err(TypeMismatch {
                        src: self.named_source(),
                        expected: String::from("struct"),
                        invalid: struct_type.to_string(),
                        span: left_node.source_span(),
                        type_def: None
                    }.into()); }
        };

        let field_name = match right_node.node_type() {
            ASTNodeType::IdentifierLiteral(value) => value,
            _ => { return Err(SyntaxError {
                src: self.named_source(),
                text: format!("Expected identifier after '{:?}', got '{:?}' instead", op.token_type(), right_node.node_type()),
                span: right_node.source_span()
            }.into()); }
        };

        let method = self.get_method(comet_struct.name().to_string(), field_name.to_string());
        if method.is_some() { // getting method
            let method = method.unwrap();
            let method_ref = self.module.declare_func_in_func(method.func_id, &mut builder.func);
            return Ok(builder.ins().func_addr(self.module.isa().pointer_type(), method_ref));
        }

        let field_index = comet_struct.get_field_index(field_name).ok_or_else(|| UnkownField {
            struct_name: String::from(comet_struct.name()),
            field: field_name.clone(),
            span: right_node.source_span(),
            src: self.named_source()
        })?;
        let field = comet_struct.get_field(field_name).unwrap();

        let comet_struct_layout = comet_struct.get_layout();
                        
        Ok(builder.ins().load(field.field_type().cranelift_type, MemFlagsData::new(), left, comet_struct_layout.0[*field_index] as i32))
    }

    fn visit_prefix_expression(&mut self, node: &ASTNode<'a>, builder: &mut FunctionBuilder) -> miette::Result<ir::Value> {
        let (op, right_node) = match node.node_type() {
            ASTNodeType::PrefixExpression { op, right } => (op, right),
            _ => unreachable!()
        };

        let right_type = self.resolve_type(right_node)?;
        let right_value = self.visit_value(right_node, builder)?;

        match op.token_type() {
            TokenType::Ampersand => {
                let stack_slot = builder.create_sized_stack_slot(StackSlotData::new(
                    StackSlotKind::ExplicitSlot,
                    right_type.size(),
                    right_type.align() as u8
                ));

                let stack_addr = builder.ins().stack_addr(self.module.isa().pointer_type(), stack_slot, 0);
                builder.ins().store(MemFlagsData::new(), right_value, stack_addr, 0);

                Ok(stack_addr)
            },

            TokenType::StarStar => {
                if !right_type.is_ptr() {
                    return Err(TypeMismatch {
                        expected: "pointer".to_string(),
                        invalid: right_type.to_string(),
                        src: self.named_source(),
                        span: right_node.source_span(),
                        type_def: None
                    }.into());
                }

                let internal_type = match right_type.kind {
                    CometTypeKind::Pointer(v) => v,
                    _ => unreachable!()
                };

                Ok(builder.ins().load(
                    internal_type.cranelift_type,
                    MemFlagsData::new(),
                    right_value,
                    0
                ))
            }

            TokenType::Minus => {

                let is_int = right_type.is_int();

                if is_int {
                    Ok(builder.ins().imul_imm_s(right_value, -1))
                } else {
                    let imm = builder.ins().f32const(-1.0);
                    Ok(builder.ins().fmul(right_value, imm))
                }

            }

            TokenType::Tilde => {
                let is_int = right_type.is_int();
                if !is_int {
                    return Err(InvalidOperator {
                        op: op.token_type().clone(),
                        span: op.source_span(),
                        src: self.named_source(),
                        value: "on non-integer".to_string()
                    }.into());
                }

                Ok(builder.ins().bnot(right_value))
            }

            TokenType::Not => {
                Ok(builder.ins().icmp_imm_s(IntCC::Equal, right_value, 0))
            }

            _ => Err(InvalidOperator {
                src: self.named_source(),
                span: op.source_span(),
                op: op.token_type().clone(),
                value: "as a prefix operator".to_string()
            }.into())
        }
    }

    fn visit_expression_statement(&mut self, node: &ASTNode<'a>, builder: &mut FunctionBuilder) -> miette::Result<()> {
        match node.node_type() {
            ASTNodeType::ExpressionStatement(expr) => {
                self.visit_value(expr, builder)?;
            },
            _ => unreachable!()
        }

        Ok(())
    }

    fn compile_function(
        &mut self,
        name: &str,
        args: &[ASTNode<'a>],
        return_type_node: Option<&ASTNode<'a>>,
        body: &ASTNode<'a>,
        owner: FunctionOwner
    ) -> miette::Result<FuncId> {
        let (is_struct_impl, symbol_name) = match owner {
            FunctionOwner::Global => (false, name.to_string().clone()),
            FunctionOwner::Impl(struct_name) => (true, format!("{struct_name}_{name}"))
        };
        let mut sig = self.module.make_signature();

        let mut builder_context = FunctionBuilderContext::new();
        let mut arg_types = Vec::new();
        let previous_function = self.current_function.clone();

        self.scopes.push(ScopeFrame::new());

        for (i, arg) in args.iter().enumerate() {
            let (arg_name_node, arg_type_node)= match arg.node_type() {
                ASTNodeType::FuncArgDefinition { name, type_ } => (name, type_),
                _ => unreachable!()
            };

            let arg_type = self.get_type_literal_type(arg_type_node)?.clone();

            let arg_name = match arg_name_node.node_type() {
                ASTNodeType::IdentifierLiteral(name) => name,
                _ => unreachable!()
            }; 

            sig.params.push(AbiParam::new(arg_type.cranelift_type));
            arg_types.push(arg_type.clone());
            self.scopes.last_mut().unwrap().variables.insert(arg_name.clone(), CometVariable {
                type_: arg_type,
                var_type: CometVarType::FuncArg(i),
                mutable: false,
                function_id: None
            });
        }

        let mut return_type = return_type_node
            .as_ref()
            .map(|node| self.get_type_literal_type(node))
            .transpose()?
            .unwrap_or_else(CometType::new_void);

        if let Some(node) = return_type_node {
            return_type = return_type.with_definition_span(Some(node.source_span()));
        }

        if return_type.cranelift_type != types::INVALID {
            sig.returns.push(AbiParam::new(return_type.cranelift_type));
        }

        let new_comet_func = CometFunction {
            arg_types,
            return_type: Box::new(return_type.clone())
        };

        self.current_function = Some(new_comet_func.clone());

        let function_type = CometType::new_function(new_comet_func);

        let func_id = self.module.declare_function(&symbol_name, Linkage::Export, &sig).unwrap();

        let mut ctx = self.module.make_context();
        ctx.func.signature = sig;
        ctx.func.name = ir::UserFuncName::user(0, func_id.as_u32());

        // build body of function //
        let mut builder = FunctionBuilder::new(&mut ctx.func, &mut builder_context);

        let entry_block = builder.create_block();
        builder.append_block_params_for_function_params(entry_block);
        builder.switch_to_block(entry_block);

        let target_config = self.module.isa().frontend_config();

        let func_ref = self.module.declare_func_in_func(func_id, builder.func);
        let func_addr = builder.ins().func_addr(target_config.pointer_type(), func_ref);

        let func_var = builder.declare_var(target_config.pointer_type());
        builder.def_var(func_var, func_addr);

        if !is_struct_impl {
            self.scopes.iter_mut().rev().nth(1).unwrap().variables.insert(symbol_name.clone(), CometVariable {
                type_: function_type,
                var_type: Local(func_var),
                mutable: false,
                function_id: Some(func_id)
            });
        }
        
        

        // generate code
        let result = match body.node_type() {
            ASTNodeType::Block(_) => {
                self.compile(body, Some(&mut builder))
            },
            _ => {
                let ret_value = self.visit_value(body, &mut builder)?;
                let function_return_type = self.current_function.as_ref().unwrap().return_type.as_ref().clone();

                if function_return_type.cranelift_type == types::INVALID {
                    builder.ins().return_(&[]);
                } else {
                    let ret_type = self.resolve_type(body)?;
                    let ret_value = if ret_type != function_return_type {
                        CometType::try_implicit_cast(
                            body,
                            ret_value,
                            &ret_type,
                            &function_return_type,
                            return_type_node,
                            &mut builder,
                            self.named_source()
                        )?
                    } else {
                        ret_value
                    };

                    builder.ins().return_(&[ret_value]);
                }
                Ok(())
            },
        };

        self.scopes.pop();
        self.current_function = previous_function;
        result?;

        // finalize func
        builder.seal_all_blocks();
        builder.finalize(target_config);

        println!("=== BUILT FUNCTION ===\n{}", ctx.func);

        self.module.define_function(func_id, &mut ctx).unwrap();
        self.module.clear_context(&mut ctx);

        Ok(func_id)
    }

    fn visit_func_def(&mut self, node: &ASTNode<'a>) -> miette::Result<()> {
        let (name_node, func_args, return_type_optional, body) = match node.node_type() {
            ASTNodeType::FuncDefinitionStatement {name, args, return_type, body } => (name, args, return_type, body),
            _ => unreachable!()
        };

        let name = match name_node.node_type() {
            ASTNodeType::IdentifierLiteral(value) => value,
            _ => unreachable!()
        };

        self.compile_function(name, func_args, return_type_optional.as_ref().map(|r| r.as_ref()), body, FunctionOwner::Global)?;

        Ok(())
        
    }

    fn visit_ret_statement(&mut self, node: &ASTNode<'a>, builder: &mut FunctionBuilder) -> miette::Result<()> {
        let ret_value_optional = match node.node_type() {
            ASTNodeType::ReturnStatement(value) => value,
            _ => unreachable!()
        };

        if !self.block_is_terminated(builder.current_block().unwrap(), builder) {
            if ret_value_optional.is_none() {
                builder.ins().return_(&[]);
            } else {
                let ret_value_node = ret_value_optional.as_ref().unwrap();
                if matches!(self.current_function.as_ref().unwrap().return_type.kind, CometTypeKind::Void) {
                    return Err(TypeMismatch {
                        expected: "void".to_string(),
                        invalid: "return value".to_string(),
                        span: ret_value_node.source_span(),
                        src: self.named_source(),
                        type_def: None
                    }.into());
                }

                let mut ret_value = self.visit_value(ret_value_node, builder)?;

                let ret_type = self.resolve_type(ret_value_node)?;
                if ret_type != *(self.current_function.as_ref().unwrap().return_type) {
                    ret_value = CometType::try_implicit_cast(ret_value_node, ret_value, &ret_type, &*(self.current_function.as_ref().unwrap().return_type), None, builder, self.named_source())?;
                }

                builder.ins().return_(&[ret_value]);
            }
        }

        Ok(())
    }

    fn visit_assign_statement(&mut self, node: &ASTNode<'a>, builder: &mut FunctionBuilder) -> miette::Result<()> {
        let (ident_node, type_node, value_node) = match node.node_type() {
            ASTNodeType::AssignStatement { ident, type_, value } => (ident, type_, value),
            _ => unreachable!()
        };

        // reassignment of struct field
        if !matches!(ident_node.node_type(), ASTNodeType::IdentifierLiteral(_)) {
            if type_node.is_some() {
                return Err(SyntaxError {
                    span: type_node.as_ref().unwrap().source_span(),
                    src: self.named_source(),
                    text: String::from("Cannot type annotate a field assignment")
                }.into());
            }

            let target_type = self.resolve_type(ident_node)?;
            let value_type = self.resolve_type(value_node)?;

            let mut value = self.visit_value(value_node, builder)?;
            let address = self.visit_l_value(ident_node, builder)?;

            if target_type != value_type {
                value = CometType::try_implicit_cast(value_node, value, &value_type, &target_type, Some(ident_node), builder, self.named_source())?;
            }

            
            builder.ins().store(MemFlagsData::new(), value, address, 0);
            return Ok(());
        }

        let ident = match ident_node.node_type() {
            ASTNodeType::IdentifierLiteral(value) => value,
            _ => unreachable!()
        };

        // get type of the value we're setting the variable to
        let value_type = self.resolve_type(value_node)?;
        
        // build variable value
        let mut value = self.visit_value(value_node, builder)?;

        let existing_var = self.get_variable(&ident.as_str());
        if existing_var.is_some() {
            if !existing_var.unwrap().mutable {
                return Err(ImmutableReassignment {
                    span: ident_node.source_span(),
                    src: self.named_source(),
                    var_name: ident.clone()
                }.into());
            }

            let var_type = &existing_var.unwrap().type_;
            if value_type != *var_type {
                return Err(TypeMismatch {
                    expected: var_type.to_string(),
                    invalid: value_type.to_string(),
                    span: value_node.source_span(),
                    src: self.named_source(),
                    type_def: Self::type_definition_span_for_error(Some(&value_type), Some(var_type))
                }.into());
            }

            if type_node.is_some() {
                return Err(SyntaxError {
                    span: type_node.as_ref().unwrap().source_span(),
                    src: self.named_source(),
                    text: String::from("Cannot type annotate reassignment")
                }.into());
            }

            match existing_var.unwrap().var_type {
                CometVarType::Local(var) => { builder.def_var(var, value); },
                _ => unreachable!()
            }

            return Ok(());
        }

        self.var_index += 1;

        match value_type.kind {
            CometTypeKind::Void => { return Err(InvalidVariableType {
                span: value_node.source_span(),
                src: self.named_source(),
                type_name: value_type.to_string()
            }.into()); },

            CometTypeKind::Unkown => {
                if type_node.is_none() { return Err(TypeAnnotationNeeded {
                    span: node.source_span(),
                    src: self.named_source(),
                    var_name: ident.clone()
                }.into()); }
            },
            _ => {}
        }
        
        let mut final_type = value_type.clone();
        if type_node.is_some() {
            let var_type = self.get_type_literal_type(type_node.as_ref().unwrap())?;

            // get type of type annotation
            if var_type != value_type {
                value = CometType::try_implicit_cast(&value_node, value, &value_type, &var_type, type_node.as_deref(), builder, self.named_source())?;
                final_type = var_type;
            } else {
                final_type = self.unify_types(&final_type, &var_type).clone();
            }
        }

        let new_variable = builder.declare_var(final_type.cranelift_type);

        builder.def_var(new_variable, value);

        let function_id = match value_node.node_type() {
            ASTNodeType::IdentifierLiteral(name) => self.get_variable(name)
                .and_then(|variable| variable.function_id),
            _ => None
        };

        let comet_var = CometVariable {
            type_: final_type,
            var_type: CometVarType::Local(new_variable),
            mutable: true,
            function_id
        };

        self.scopes.last_mut().unwrap().variables.insert(ident.clone(), comet_var);

        Ok(())
    }

    fn pattern_variant_item(
        &mut self,
        path: &[ASTNode<'a>],
        expected_type: &CometType,
        span: miette::SourceSpan
    ) -> miette::Result<CometUnionItem> {
        if path.len() != 2 {
            return Err(SyntaxError {
                span,
                src: self.named_source(),
                text: "variant pattern path must have the form Union<Type>::Variant".to_string()
            }.into());
        }

        let (union_name, generic_types) = match path[0].node_type() {
            ASTNodeType::QualifierNode { ident, generics } => {
                let name = match ident.node_type() {
                    ASTNodeType::IdentifierLiteral(name) => name,
                    _ => unreachable!()
                };
                (name.clone(), generics.as_deref())
            }
            _ => {
                return Err(SyntaxError {
                    span: path[0].source_span(),
                    src: self.named_source(),
                    text: "expected a union type in variant pattern".to_string()
                }.into());
            }
        };

        let union_type = if let Some(generic_types) = generic_types {
            self.get_generic_type_literal_type(&path[0], generic_types)?
        } else {
            self.get_type(&union_name).cloned().ok_or_else(|| UnkownType {
                span: path[0].source_span(),
                src: self.named_source(),
                type_: union_name.clone()
            })?
        };

        let union = match union_type.kind {
            CometTypeKind::Union(union) => union,
            _ => {
                return Err(TypeMismatch {
                    expected: "union".to_string(),
                    invalid: union_type.to_string(),
                    span: path[0].source_span(),
                    src: self.named_source(),
                    type_def: None
                }.into());
            }
        };

        let variant_name = match path[1].node_type() {
            ASTNodeType::QualifierNode { ident, generics: None } => match ident.node_type() {
                ASTNodeType::IdentifierLiteral(name) => name,
                _ => unreachable!()
            },
            ASTNodeType::QualifierNode { generics: Some(_), .. } => {
                return Err(SyntaxError {
                    span: path[1].source_span(),
                    src: self.named_source(),
                    text: "generic arguments are only allowed on the union type".to_string()
                }.into());
            }
            _ => {
                return Err(SyntaxError {
                    span: path[1].source_span(),
                    src: self.named_source(),
                    text: "expected a variant name".to_string()
                }.into());
            }
        };

        let item = union.get_item(variant_name).cloned().ok_or_else(|| UnkownUnionItem {
            item: variant_name.clone(),
            union: union.name().clone(),
            span: path[1].source_span(),
            src: self.named_source(),
        })?;

        // Nested patterns must use the same specialized union as their payload.
        let expected_union = match &expected_type.kind {
            CometTypeKind::Union(union) => union.name().as_str(),
            CometTypeKind::Variant(variant) => variant.union_name(),
            _ => {
                return Err(TypeMismatch {
                    expected: "union".to_string(),
                    invalid: expected_type.to_string(),
                    span,
                    src: self.named_source(),
                    type_def: None
                }.into());
            }
        };

        if item.union_name() != union.name() || item.union_name() != expected_union {
            return Err(TypeMismatch {
                expected: expected_union.to_string(),
                invalid: item.union_name().to_string(),
                span,
                src: self.named_source(),
                type_def: None
            }.into());
        }

        Ok(item)
    }

    /*
    Recursively checks that each pattern belongs to the given union.
     */
    fn collect_pattern_bindings(
        &mut self,
        pattern: &MatchPattern<'a>,
        expected_type: &CometType
    ) -> miette::Result<Vec<(String, CometType)>> {
        match pattern {
            MatchPattern::Variant { path, fields } => {
                let item = self.pattern_variant_item(path, expected_type, path[0].source_span())?;
                if fields.len() != item.field_names().len() {
                    return Err(SyntaxError {
                        span: path.last().unwrap().source_span(),
                        src: self.named_source(),
                        text: format!("variant '{}' expects {} pattern fields, got {}", item.name(), item.field_names().len(), fields.len())
                    }.into());
                }

                let mut bindings = Vec::new();
                for (index, field_pattern) in fields.iter().enumerate() {
                    let field_type = item.field_type_at(index).unwrap();
                    match field_pattern {
                        // go through names and add to scope
                        MatchPattern::Binding(binding) => {
                            let name = match binding.node_type() {
                                ASTNodeType::IdentifierLiteral(name) => name,
                                _ => unreachable!()
                            };
                            if name != "_" {
                                bindings.push((name.clone(), field_type.clone()));
                            }
                        }
                        MatchPattern::Variant { .. } => {
                            // visit inner node
                            bindings.extend(self.collect_pattern_bindings(field_pattern, field_type)?);
                        }
                        MatchPattern::Expression(expression) => {
                            return Err(SyntaxError {
                                span: expression.source_span(),
                                src: self.named_source(),
                                text: "variant payload patterns must be bindings or nested variant patterns".to_string()
                            }.into());
                        }
                    }
                }
                Ok(bindings)
            }
            MatchPattern::Expression(_) => Ok(Vec::new()),
            MatchPattern::Binding(binding) => Err(SyntaxError {
                span: binding.source_span(),
                src: self.named_source(),
                text: "a binding must appear inside a variant pattern".to_string()
            }.into())
        }
    }

    fn emit_variant_match_branch(
        &mut self,
        pattern: &MatchPattern<'a>,
        value: ir::Value,
        value_type: &CometType,
        matched_block: Block,
        failed_block: Block,
        builder: &mut FunctionBuilder
    ) -> miette::Result<()> {

        /* emit code for the switch branch that handles a variant */

        // get path and fields
        let (path, fields) = match pattern {
            MatchPattern::Variant { path, fields } => (path, fields),
            _ => unreachable!()
        };

        // get item
        let item = self.pattern_variant_item(path, value_type, path[0].source_span())?;
        let nested_fields = fields.iter().enumerate().filter_map(|(index, field_pattern)| {
            match field_pattern {
                MatchPattern::Variant { .. } => Some((index, field_pattern)),
                _ => None
            }
        }).collect::<Vec<_>>();

        // get the discriminant
        let discriminant = builder.ins().load(types::I32, MemFlagsData::new(), value, 0);
        let is_variant = builder.ins().icmp_imm_s(IntCC::Equal, discriminant, item.discriminant() as i64);
        let current_block = builder.current_block().unwrap();

        // if the variant has no nested fields just chec the discriminant matches
        if nested_fields.is_empty() {
            builder.ins().brif(is_variant, matched_block, &[], failed_block, &[]);
            builder.seal_block(current_block);
            return Ok(());
        }

        // check the discriminant matches, if it does then go to the nested checks
        let nested_check_block = builder.create_block();
        builder.ins().brif(is_variant, nested_check_block, &[], failed_block, &[]);
        builder.seal_block(current_block);
        builder.switch_to_block(nested_check_block);

        for (nested_index, (field_index, field_pattern)) in nested_fields.iter().enumerate() {
            let field_name = item.field_name_at(*field_index).unwrap();
            let field_type = item.field_type_at(*field_index).unwrap().clone();
            let field_offset = item.payload_offset(types::I32.bytes()) + item.field_offset(field_name).unwrap();
            let field_address = builder.ins().iadd_imm_s(value, field_offset as i64);
            let field_value = builder.ins().load(field_type.cranelift_type, MemFlagsData::new(), field_address, 0);
            let next_block = if nested_index + 1 == nested_fields.len() {
                matched_block
            } else {
                builder.create_block()
            };

            self.emit_variant_match_branch(field_pattern, field_value, &field_type, next_block, failed_block, builder)?;

            if next_block != matched_block {
                builder.switch_to_block(next_block);
            }
        }

        Ok(())
    }

    fn bind_variant_pattern(
        &mut self,
        pattern: &MatchPattern<'a>,
        value: ir::Value,
        value_type: &CometType,
        variables: &HashMap<String, (Variable, CometType)>,
        builder: &mut FunctionBuilder
    ) -> miette::Result<()> {
        /* define variables for a pattern */

        // get the patha nd itme
        let (path, fields) = match pattern {
            MatchPattern::Variant { path, fields } => (path, fields),
            _ => unreachable!()
        };
        let item = self.pattern_variant_item(path, value_type, path[0].source_span())?;

        // go through each field in the pattern
        for (index, field_pattern) in fields.iter().enumerate() {
            let field_name = item.field_name_at(index).unwrap();
            let field_type = item.field_type_at(index).unwrap().clone();
            let field_offset = item.payload_offset(types::I32.bytes()) + item.field_offset(field_name).unwrap();
            let field_address = builder.ins().iadd_imm_s(value, field_offset as i64);
            let field_value = builder.ins().load(field_type.cranelift_type, MemFlagsData::new(), field_address, 0);

            // define a variable for each field
            match field_pattern {
                MatchPattern::Binding(binding) => {
                    let name = match binding.node_type() {
                        ASTNodeType::IdentifierLiteral(name) => name,
                        _ => unreachable!()
                    };
                    if name != "_" {
                        let (variable, _) = variables.get(name).ok_or_else(|| CompilerBug {
                            span: binding.source_span(),
                            src: self.named_source(),
                            text: format!("missing match binding '{}'", name)
                        })?;
                        builder.def_var(*variable, field_value);
                    }
                }
                MatchPattern::Variant { .. } => {
                    self.bind_variant_pattern(field_pattern, field_value, &field_type, variables, builder)?;
                }
                MatchPattern::Expression(expression) => {
                    return Err(SyntaxError {
                        span: expression.source_span(),
                        src: self.named_source(),
                        text: "variant payload patterns must be bindings or nested variant patterns".to_string()
                    }.into());
                }
            }
        }

        Ok(())
    }

    fn pattern_is_unconditional_variant(pattern: &MatchPattern<'a>) -> bool {
        /* returns true if a variant carries no data */
        match pattern {
            MatchPattern::Variant { fields, .. } => fields.iter().all(|field| matches!(field, MatchPattern::Binding(_))),
            _ => false
        }
    }

    fn pattern_key(pattern: &MatchPattern<'a>) -> Option<String> {
        match pattern {
            MatchPattern::Variant { path, fields } => {
                let names = path.iter().map(|node| match node.node_type() {
                    ASTNodeType::IdentifierLiteral(name) => Some(name.as_str()),
                    _ => None
                }).collect::<Option<Vec<_>>>()?;
                let fields = fields.iter().map(|field| match field {
                    MatchPattern::Binding(_) => Some("_".to_string()),
                    MatchPattern::Variant { .. } => Self::pattern_key(field),
                    MatchPattern::Expression(_) => None
                }).collect::<Option<Vec<_>>>()?;
                Some(format!("{}({})", names.join("::"), fields.join(",")))
            }
            _ => None
        }
    }

    fn visit_match_statement(&mut self, node: &ASTNode<'a>, builder: &mut FunctionBuilder) -> miette::Result<()> {
        let (expr_node, match_nodes, default_branch) = match node.node_type() {
            ASTNodeType::MatchStatement { expr, nodes, default } => (expr, nodes, default),
            _ => unreachable!()
        };

        // build scrutinee type and value. for a union, `expr` is the address
        // of its storage
        let scrutinee_type = self.resolve_type(expr_node)?;
        let expr = self.visit_value(expr_node, builder)?;
        let union = match &scrutinee_type.kind {
            CometTypeKind::Union(union) => Some(union.clone()),
            _ => None
        };

        let mut binding_signatures = Vec::new();
        let mut completely_covered = HashSet::new();
        let mut seen_patterns = HashSet::new();

        // do a check of each arm first
        for match_node in match_nodes {
            let patterns = match match_node.node_type() {
                ASTNodeType::MatchNode { expressions, .. } => expressions,
                _ => unreachable!()
            };
            let mut arm_signature: Option<Vec<(String, CometType)>> = None;

            for pattern in patterns {
                if let Some(key) = Self::pattern_key(pattern) {
                    if !seen_patterns.insert(key) {
                        return Err(SyntaxError {
                            span: match_node.source_span(),
                            src: self.named_source(),
                            text: "this variant pattern is already matched".to_string()
                        }.into());
                    }
                }

                // if we're checking a union then each union arm must be
                // checking a pattern, not an expression
                if union.is_some() && !matches!(pattern, MatchPattern::Variant { .. }) {
                    return Err(SyntaxError {
                        span: match_node.source_span(),
                        src: self.named_source(),
                        text: "union values must be matched with variant patterns or a default arm".to_string()
                    }.into());
                }

                // get bindings, adding each name
                let signature = self.collect_pattern_bindings(pattern, &scrutinee_type)?;
                let mut names = HashSet::new();

                // check for duplicate binding names
                for (name, _) in &signature {
                    if !names.insert(name.clone()) {
                        return Err(SyntaxError {
                            span: match_node.source_span(),
                            src: self.named_source(),
                            text: format!("binding '{}' appears more than once in this pattern", name)
                        }.into());
                    }
                }

                if let Some(existing) = &arm_signature {
                    if existing.len() != signature.len() || existing.iter().zip(signature.iter()).any(|(left, right)| left.0 != right.0 || left.1 != right.1) {
                        return Err(SyntaxError {
                            span: match_node.source_span(),
                            src: self.named_source(),
                            text: "all alternatives in one match arm must bind the same names and types".to_string()
                        }.into());
                    }
                } else {
                    arm_signature = Some(signature);
                }

                // go through patterns with no data
                if Self::pattern_is_unconditional_variant(pattern) {
                    if let MatchPattern::Variant { path, .. } = pattern {

                        // get item from path
                        let item = self.pattern_variant_item(path, &scrutinee_type, path[0].source_span())?;
                        if !completely_covered.insert(item.discriminant()) {
                            return Err(SyntaxError {
                                span: path.last().unwrap().source_span(),
                                src: self.named_source(),
                                text: format!("variant '{}::{}' is matched more than once", item.union_name(), item.name())
                            }.into());
                        }
                    }
                }
            }

            binding_signatures.push(arm_signature.unwrap_or_default());
        }

        // if the union exists and there is no default branch, make sure there
        // is enough branches to cover all variants
        if let Some(union) = &union {
            if default_branch.is_none() && completely_covered.len() != union.items().len() {
                let missing = union.items().iter()
                    .filter(|item| !completely_covered.contains(&item.discriminant()))
                    .map(|item| item.name().as_str())
                    .collect::<Vec<_>>()
                    .join(", ");
                return Err(SyntaxError {
                    span: node.source_span(),
                    src: self.named_source(),
                    text: format!("non-exhaustive union match; add patterns for: {} or provide a default arm", missing)
                }.into());
            }
        }

        // go through usual match statement logic for non variants
        let end_block = builder.create_block();
        let default_block = default_branch.as_ref().map(|_| builder.create_block());
        let mut compare_block = builder.current_block().unwrap();
        let mut end_reachable = default_branch.is_none() && union.is_none();

        for (arm_index, match_node) in match_nodes.iter().enumerate() {
            let (patterns, match_block) = match match_node.node_type() {
                ASTNodeType::MatchNode { expressions, block } => (expressions, block),
                _ => unreachable!()
            };
            let arm_block = builder.create_block();
            let mut variables = HashMap::new();

            for (name, type_) in &binding_signatures[arm_index] {
                let variable = builder.declare_var(type_.cranelift_type);
                variables.insert(name.clone(), (variable, type_.clone()));
            }

            for (pattern_index, pattern) in patterns.iter().enumerate() {
                if builder.current_block() != Some(compare_block) {
                    builder.switch_to_block(compare_block);
                }

                let failed_block = if pattern_index + 1 < patterns.len() {
                    builder.create_block()
                } else if arm_index + 1 < match_nodes.len() {
                    builder.create_block()
                } else {
                    default_block.unwrap_or(end_block)
                };
                let pattern_block = builder.create_block();

                match pattern {
                    MatchPattern::Variant { .. } => {
                        // if its a variant do something special for that
                        self.emit_variant_match_branch(pattern, expr, &scrutinee_type, pattern_block, failed_block, builder)?;
                    }
                    MatchPattern::Expression(value_node) => {
                        let pattern_value = self.visit_value(value_node, builder)?;
                        let pattern_type = self.resolve_type(value_node)?;
                        let is_equal = if scrutinee_type.is_float() && pattern_type.is_float() {
                            builder.ins().fcmp(FloatCC::Equal, expr, pattern_value)
                        } else {
                            builder.ins().icmp(IntCC::Equal, expr, pattern_value)
                        };
                        builder.ins().brif(is_equal, pattern_block, &[], failed_block, &[]);
                        builder.seal_block(compare_block);
                    }
                    MatchPattern::Binding(binding) => {
                        return Err(SyntaxError {
                            span: binding.source_span(),
                            src: self.named_source(),
                            text: "a binding must appear inside a variant pattern".to_string()
                        }.into());
                    }
                }

                builder.switch_to_block(pattern_block);

                // if the pattern is a variant then define all its field variables
                if matches!(pattern, MatchPattern::Variant { .. }) {
                    self.bind_variant_pattern(pattern, expr, &scrutinee_type, &variables, builder)?;
                }
                builder.ins().jump(arm_block, &[]);
                builder.seal_block(pattern_block);

                if pattern_index + 1 < patterns.len() || arm_index + 1 < match_nodes.len() {
                    compare_block = failed_block;
                }
            }

            builder.switch_to_block(arm_block);
            self.scopes.push(ScopeFrame::new());
            for (name, (variable, type_)) in variables {
                self.scopes.last_mut().unwrap().variables.insert(name, CometVariable {
                    type_,
                    var_type: CometVarType::Local(variable),
                    mutable: false,
                    function_id: None
                });
            }
            let compile_result = self.compile(match_block, Some(builder));
            self.scopes.pop();
            compile_result?;

            let arm_is_terminated = self.block_is_terminated(arm_block, builder);
            if !arm_is_terminated {
                builder.ins().jump(end_block, &[]);
                end_reachable = true;
            }
        }

        // if the default branch exists...
        if let Some(default_branch) = default_branch {
            let default_block = default_block.unwrap();
            builder.switch_to_block(default_block);
            self.compile(default_branch, Some(builder))?;

            let default_is_terminated = self.block_is_terminated(default_block, builder);
            if !default_is_terminated {
                builder.ins().jump(end_block, &[]);
                end_reachable = true;
            }
            builder.seal_block(default_block);
        }

        builder.switch_to_block(end_block);
        if end_reachable {
            builder.ensure_inserted_block();
        } else {
            builder.ins().trap(ir::TrapCode::unwrap_user(1));
        }
        Ok(())
    }

    fn block_is_terminated(&self, block: Block, builder: &mut FunctionBuilder) -> bool {
        let is_terminated = builder
                    .func
                    .layout
                    .last_inst(block)
                    .map(|inst| builder.func.dfg.insts[inst].opcode().is_terminator())
                    .unwrap_or(false);

        return is_terminated;
    }

    fn visit_if_statement(&mut self, node: &ASTNode<'a>, builder: &mut FunctionBuilder) -> miette::Result<()> {
        let (expr_node, body_node, else_body) = match node.node_type() {
            ASTNodeType::IfStatement { expr, body, else_body } => (expr, body, else_body),
            _ => unreachable!()
        };

        let expr = self.visit_value(expr_node, builder)?;

        let then_block = builder.create_block();
        let else_block = builder.create_block();
        let end_block = else_body.as_ref().map(|_| builder.create_block()).unwrap_or(else_block);

        builder.ins().brif(expr, then_block, &[], else_block, &[]);
        builder.seal_block(builder.current_block().unwrap());

        builder.switch_to_block(then_block);
        builder.ensure_inserted_block();
        self.compile(body_node, Some(builder))?;

        let then_block_terminated = self.block_is_terminated(then_block, builder);
        if !then_block_terminated {
            builder.ins().jump(end_block, &[]);
        }

        builder.switch_to_block(else_block);
        if else_body.is_some() {
            self.compile(else_body.as_ref().unwrap(), Some(builder))?;

            let else_block_terminated = self.block_is_terminated(else_block, builder);
            if !else_block_terminated {
                builder.ins().jump(end_block, &[]);
            }

            builder.switch_to_block(end_block);
            if then_block_terminated && else_block_terminated {
                builder.ins().trap(ir::TrapCode::unwrap_user(1));
                builder.seal_block(end_block);
            } else {
                builder.ensure_inserted_block();
            }
        } else {
            builder.ensure_inserted_block();
        }

        Ok(())
    }

    fn visit_while_statement(&mut self, node: &ASTNode<'a>, builder: &mut FunctionBuilder) -> miette::Result<()> {
        let (expr_node, body_node) = match node.node_type() {
            ASTNodeType::WhileStatement { expr, body } => (expr, body),
            _ => unreachable!()
        };

        let compare_block = builder.create_block();
        let body_block = builder.create_block();
        let end_block = builder.create_block();

        builder.ins().jump(compare_block, &[]);
        builder.seal_block(builder.current_block().unwrap());
        builder.switch_to_block(compare_block);
        let expr = self.visit_value(expr_node, builder)?;
        builder.ins().brif(expr, body_block, &[], end_block, &[]);

        builder.switch_to_block(body_block);

        self.compile(body_node, Some(builder))?;

        builder.ins().jump(compare_block, &[]);
        builder.seal_block(compare_block);

        builder.seal_block(body_block);
        builder.switch_to_block(end_block);

        Ok(())
    }


    fn visit_struct_def_statement(&mut self, node: &ASTNode<'a>) -> miette::Result<Option<CometStruct>> {
        let (ident_node, field_nodes, generics) = match node.node_type() {
            ASTNodeType::StructDefinitionStatement { ident, fields, generics } => (ident, fields, generics),
            _ => unreachable!()
        };

        let ident = match ident_node.node_type() {
            ASTNodeType::IdentifierLiteral(value) => value,
            _ => unreachable!()
        };

        if generics.is_some() {
            let generic_template = node.clone();
            self.scopes.last_mut().unwrap().generics.insert(ident.clone(), generic_template);
            return Ok(None);
        }

        let mut fields: Vec<CometStructField> = Vec::new();

        // predeclare the empty struct so the user can have types in themself
        let base_struct = CometStruct::new(ident.clone(), vec![]);
        self.scopes.last_mut().unwrap().types.insert(
            ident.clone(),
            CometType::new_struct(base_struct.clone()).with_definition_span(Some(ident_node.source_span()))
        );

        for field_node in field_nodes {
            let (field_ident_node, field_type_node) = match field_node.node_type() {
                ASTNodeType::StructFieldDefinition { ident, type_ } => (ident, type_),
                _ => unreachable!()
            };

            let field_name = match field_ident_node.node_type() {
                ASTNodeType::IdentifierLiteral(value) => value,
                _ => unreachable!()
            };

            let field_type = self.get_type_literal_type(field_type_node)?;

            fields.push(CometStructField::new(field_name.clone(), field_type.clone()));
        }

        let new_struct = CometStruct::new(ident.clone(), fields);

        self.scopes.last_mut().unwrap().types.insert(
            ident.clone(),
            CometType::new_struct(new_struct.clone()).with_definition_span(Some(ident_node.source_span()))
        );

        Ok(Some(new_struct))
    }

    fn visit_union_def_statement(&mut self, node: &ASTNode<'a>, resolved_generics: Option<&Vec<CometType>>) -> miette::Result<Option<CometUnion>> {
        let (ident_node, item_nodes, generics) = match node.node_type() {
            ASTNodeType::UnionDefinitionStatement { ident, fields, generics } => (ident, fields, generics),
            _ => unreachable!()
        };

        let union_name = match ident_node.node_type() {
            ASTNodeType::IdentifierLiteral(v) => v,
            _ => unreachable!()
        };

        let ident = if resolved_generics.is_some() {
            &CometUnion::get_mangled_name(union_name, resolved_generics.unwrap())
        } else {
            union_name
        };

        if generics.is_some() {
            let generic_template = node.clone();
            self.scopes.last_mut().unwrap().generics.insert(ident.clone(), generic_template);
            return Ok(None);
        }

        self.scopes.last_mut().unwrap().types.insert(
            ident.clone(),
            CometType::new_union(CometUnion::new(ident.clone(), Vec::new())).with_definition_span(Some(ident_node.source_span()))
        );

        let mut items = Vec::new();
        let mut variant_names = HashSet::new();
        for (i, item_node) in item_nodes.iter().enumerate() {
            let (item_ident_node, item_names, item_types) = match item_node.node_type() {
                ASTNodeType::UnionItemDefinition { ident, names, types } => (ident, names, types),
                _ => unreachable!()
            };

            let item_ident = match item_ident_node.node_type() {
                ASTNodeType::IdentifierLiteral(v) => v,
                _ => unreachable!()
            };

            if !variant_names.insert(item_ident.clone()) {
                return Err(SyntaxError {
                    span: item_ident_node.source_span(),
                    src: self.named_source(),
                    text: format!("variant '{}' is declared more than once", item_ident)
                }.into());
            }

            let mut fields = Vec::new();
            let mut field_names = HashSet::new();
            for (name_node, type_node) in zip(item_names, item_types) {
                let field_name = match name_node.node_type() {
                    ASTNodeType::IdentifierLiteral(v) => v,
                    _ => unreachable!()
                };

                if !field_names.insert(field_name.clone()) {
                    return Err(SyntaxError {
                        span: name_node.source_span(),
                        src: self.named_source(),
                        text: format!("payload field '{}' is declared more than once", field_name)
                    }.into());
                }

                let comet_field_type = self.get_type_literal_type(type_node)?;

                fields.push((field_name.clone(), comet_field_type));
            }

            let union_item = CometUnionItem::new(
                ident.clone(),
                item_ident.clone(),
                fields,
                i as u32
            );

            items.push(union_item.clone());
            self.scopes.last_mut().unwrap().types.insert(format!("{}::{}", ident, item_ident), CometType::new_variant(union_item));
            

        }

        let new_union = CometUnion::new(ident.clone(), items);
        self.scopes.last_mut().unwrap().types.insert(
            ident.clone(),
            CometType::new_union(new_union.clone()).with_definition_span(Some(ident_node.source_span()))
        );

        Ok(Some(new_union))
    }

    fn visit_impl_def_statement(&mut self, node: &ASTNode<'a>) -> miette::Result<()> {
        let (struct_node, functions) = match node.node_type() {
            ASTNodeType::ImplDefStatement { struct_type, functions } => (struct_type, functions),
            _ => unreachable!()
        };

        let is_generic_impl = match struct_node.node_type() {
            ASTNodeType::TypeLiteral { generic_types, .. } => generic_types.is_some(),
            _ => false
        };
        if is_generic_impl {
            self.generic_impls.push(node.clone());
            return Ok(());
        }

        let struct_type = self.get_type_literal_type(struct_node)?;

        self.scopes.last_mut().unwrap().types.insert("Self".to_string(), struct_type.clone());

        let comet_struct = match struct_type.kind {
            CometTypeKind::Struct(s) => s,
            _ => { return Err(TypeMismatch {
                invalid: struct_type.to_string(),
                expected: "struct".to_string(),
                span: struct_node.source_span(),
                src: self.named_source(),
                type_def: None
            }.into()) }
        };


        for function in functions {
            let (name_node, func_args, return_type_optional, body) = match function.node_type() {
                ASTNodeType::FuncDefinitionStatement {name, args, return_type, body } => (name, args, return_type, body),
                _ => unreachable!()
            };

            let name = match name_node.node_type() {
                ASTNodeType::IdentifierLiteral(value) => value,
                _ => unreachable!()
            };

            let func_owner = FunctionOwner::Impl(String::from(comet_struct.name()));
            let func_id = self.compile_function(name, func_args, return_type_optional.as_ref().map(|r| r.as_ref()), body, func_owner)?;
        
            let mut arg_types = Vec::new();
            for func_arg in func_args {
                let arg_type = match func_arg.node_type() {
                    ASTNodeType::FuncArgDefinition { name: _, type_ } => self.get_type_literal_type(type_)?,
                    _ => unreachable!()
                };

                arg_types.push(arg_type);
            }

            let return_type = return_type_optional.as_ref().map(|t| self.get_type_literal_type(&*t))
                .transpose()?
                .unwrap_or(CometType::new_void());

            let function = CometFunction {
                arg_types: arg_types,
                return_type: Box::new(return_type)
            };

            self.methods.insert((comet_struct.name().to_string(), name.clone()), CometMethod {
                function: function,
                func_id: func_id
            });
        }

        Ok(())
    }

    fn visit_extern_directive(&mut self, name_node: &ASTNode<'a>, type_node: &ASTNode<'a>) -> miette::Result<()> {
        let name = match name_node.node_type() {
            ASTNodeType::IdentifierLiteral(value) => value,
            _ => unreachable!()
        };

        // make signature
        let mut sig = self.module.make_signature();
        let comet_type = self.get_type_literal_type(type_node)?;

        let func_type = match &comet_type.kind {
            CometTypeKind::Function(v) => v,
            _ => { return Err(CompilerBug {
                src: self.named_source(),
                span: type_node.source_span(),
                text: String::from("only functions are supported with #extern")
            }.into()); }
        };

        if func_type.return_type.cranelift_type != types::INVALID {
            sig.returns.push(AbiParam::new(func_type.return_type.cranelift_type));
        }
        for arg in &func_type.arg_types {
            sig.params.push(AbiParam::new(arg.cranelift_type))
        }

        // make external func with new sig
        let ext_func_id = self.module
            .declare_function(name, Linkage::Import, &sig)
            .unwrap();

        self.scopes.last_mut().unwrap().variables.insert(name.clone(), CometVariable {
            type_: comet_type,
            var_type: CometVarType::External(ext_func_id),
            mutable: false,
            function_id: Some(ext_func_id)
        });

        Ok(())
    }

    fn visit_compiler_directive(&mut self, node: &ASTNode<'a>) -> miette::Result<()> {
        let (directive_node, name_node, type_node) = match node.node_type() {
            ASTNodeType::CompilerDirectiveStatement { directive, value_name, value_type } => (directive, value_name, value_type),
            _ => unreachable!()
        };

        let directive = match directive_node.node_type() {
            ASTNodeType::IdentifierLiteral(v) => v,
            _ => unreachable!()
        };

        match directive.as_str() {
            "ext" => {
                self.visit_extern_directive(name_node, type_node)
            },
            _ => Err(InvalidCompilerDirective {
                directive: directive.clone(),
                span: directive_node.source_span(),
                src: self.named_source()
            }.into())
        }

    }

    fn visit_bring_statement(&mut self, node: &ASTNode<'a>) -> miette::Result<()> {
        let (path_nodes, as_node_optional) = match node.node_type() {
            ASTNodeType::BringStatement { path, as_ } => (path, as_),
            _ => unreachable!()
        };

        let as_node = as_node_optional.as_ref();

        let name = as_node.map(|n| match n.node_type() {
            ASTNodeType::IdentifierLiteral(v) => v.clone(),
            _ => unreachable!()
        }).unwrap_or(match path_nodes.iter().last().unwrap().node_type() {
            ASTNodeType::QualifierNode { ident, .. } => {
                match ident.node_type() {
                    ASTNodeType::IdentifierLiteral(v) => v.clone(),
                    _ => unreachable!()
                }
            },
            _ => unreachable!()
        });

        let path: String = path_nodes
                    .iter()
                    .map(|n| match n.node_type() {
                        ASTNodeType::QualifierNode { ident, .. } => {
                            match ident.node_type() {
                                ASTNodeType::IdentifierLiteral(v) => v,
                                _ => unreachable!()
                            }
                        },
                        _ => unreachable!()
                    })
                    .join("/") + ".comet";

        

        let include_source = fs::read_to_string(&path).into_diagnostic()?;

        let mut include_lexer = lexer::Lexer::new(&path, &include_source);

        let include_tokens = include_lexer.lex()?;

        let mut include_parser = parser::Parser::new(include_tokens);
        let include_ast = include_parser.parse()?;

        let mut include_compiler = Compiler::new(&path, include_source.clone()).unwrap();
        include_compiler.compile(&include_ast, None)?;

        let included_vars = &include_compiler.scopes[0].variables;

        let mut include_scope = scope::ScopeFrame::new();

        // import functions
        for (name, value) in included_vars {
            let comet_type = &value.type_;

            match &comet_type.kind {
                CometTypeKind::Function(func_type) => {
                    // make signature
                    let mut sig = self.module.make_signature();

                    if func_type.return_type.cranelift_type != types::INVALID {
                        sig.returns.push(AbiParam::new(func_type.return_type.cranelift_type));
                    }
                    for arg in &func_type.arg_types {
                        sig.params.push(AbiParam::new(arg.cranelift_type))
                    }

                    // make external func with new sig
                    let ext_func_id = self.module
                        .declare_function(name, Linkage::Import, &sig)
                        .unwrap();

                    include_scope.variables.insert(name.clone(), CometVariable {
                        type_: comet_type.clone(),
                        var_type: CometVarType::External(ext_func_id),
                        mutable: false,
                        function_id: Some(ext_func_id)
                    });
                },
                _ => {}
            }
        }

        // import types
        let included_types = &include_compiler.scopes[0].types;
        for (name, type_) in included_types {
            include_scope.types.insert(name.clone(), type_.clone());
        }


        self.scopes.iter_mut().last().unwrap().variables.insert(name.clone(), CometVariable { 
            type_: CometType { cranelift_type: types::INVALID, kind: CometTypeKind::Module, definition_span: None },
            var_type: CometVarType::Module(include_scope),
            function_id: None,
            mutable: false
         });


        Ok(())
    }
    // END OF VISIT METHODS //

    pub fn compile(&mut self, ast: &ASTNode<'a>, builder: Option<&mut FunctionBuilder>) -> miette::Result<()> {

        match ast.node_type() {
            ASTNodeType::Program(_) => { return self.visit_program(ast); },
            ASTNodeType::Block(_) => { return self.visit_block(ast, builder.unwrap()); },

            ASTNodeType::FuncDefinitionStatement { .. } => { return self.visit_func_def(ast); },
            ASTNodeType::ExpressionStatement(_) => { return self.visit_expression_statement(ast, builder.unwrap()); },
            ASTNodeType::ReturnStatement(_) => { return self.visit_ret_statement(ast, builder.unwrap()); }
            ASTNodeType::AssignStatement { .. } => { return self.visit_assign_statement(ast, builder.unwrap()); }
            ASTNodeType::MatchStatement { .. } => { return self.visit_match_statement(ast, builder.unwrap()); },
            ASTNodeType::IfStatement { .. } => { return self.visit_if_statement(ast, builder.unwrap()) },
            ASTNodeType::WhileStatement { .. } => { return self.visit_while_statement(ast, builder.unwrap()); },
            ASTNodeType::StructDefinitionStatement { .. } => {
                self.visit_struct_def_statement(ast)?;
                return Ok(());
            },
            ASTNodeType::UnionDefinitionStatement { .. } => {
                self.visit_union_def_statement(ast, None)?;
                return Ok(());
            }
            ASTNodeType::ImplDefStatement { .. } => { return self.visit_impl_def_statement(ast); },
            ASTNodeType::BringStatement { .. } => { return self.visit_bring_statement(ast); },

            ASTNodeType::CompilerDirectiveStatement { .. } => { return self.visit_compiler_directive(ast); },

            _ => {
                Err(CompilerBug {
                    span: ast.source_span(),
                    src: self.named_source(),
                    text: format!("No compiler visit method for {:?}", ast.node_type())
                }.into())
            }
        }
    }
}