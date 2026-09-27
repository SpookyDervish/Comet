use std::cmp;
use std::collections::HashMap;
use itertools::Itertools;

use crate::comet_type::CometType;

#[derive(Debug, Clone, Eq, PartialEq)]
pub struct CometUnionItem {
    union_name: String,
    name: String,
    fields: HashMap<String, CometType>,
    field_order: Vec<String>,
    discriminant: u32
}

#[derive(Debug, Clone, Eq, PartialEq)]
pub struct CometUnion {
    name: String,
    items: Vec<CometUnionItem>,
    cached_items: HashMap<String, usize>
}

impl CometUnion {
    pub fn new(name: String, items: Vec<CometUnionItem>) -> Self {
        let mut cached_items = HashMap::new();

        for (idx, field) in items.iter().enumerate() {
            cached_items.insert(field.name().clone(), idx);
        }

        CometUnion {
            items: items,
            name: name,
            cached_items: cached_items
        }
    }

    pub fn get_item_index(&self, name: &String) -> Option<usize> {
        return self.cached_items.get(name).copied();
    }

    pub fn get_item(&self, name: &String) -> Option<&CometUnionItem> {
        return self.get_item_index(name).map(|i| &self.items[i]);
    }

    pub fn items(&self) -> &[CometUnionItem] {
        &self.items
    }

    pub fn storage_size(&self, discriminant_size: u32) -> u32 {
        self.items.iter()
            .map(|item| item.payload_offset(discriminant_size) + item.size())
            .max()
            .unwrap_or(discriminant_size)
    }

    pub fn storage_align(&self, discriminant_size: u32) -> u32 {
        self.items.iter()
            .map(|item| item.align().max(discriminant_size))
            .max()
            .unwrap_or(discriminant_size)
    }

    pub fn name(&self) -> &String {
        &self.name
    }

    pub fn get_mangled_name(struct_name: &str, types: &Vec<CometType>) -> String {
        format!("{}__{}", struct_name, types.iter().map(|t| t.generic_type_name()).join("__"))
    }

    pub fn mangle_name(&mut self, types: &Vec<CometType>) {
        self.name = CometUnion::get_mangled_name(self.name(), types);
    }
}

impl CometUnionItem {
    pub fn new(union_name: String, name: String, fields: Vec<(String, CometType)>, discriminant: u32) -> Self {
        let field_order = fields.iter().map(|(name, _)| name.clone()).collect();
        let fields = fields.into_iter().collect();

        CometUnionItem {
            union_name,
            name,
            fields,
            field_order,
            discriminant
        }
    }

    pub fn union_name(&self) -> &str {
        &self.union_name
    }

    pub fn discriminant(&self) -> u32 {
        self.discriminant
    }

    pub fn name(&self) -> &String {
        &self.name
    }

    pub fn get_field(&self, name: &str) -> Option<&CometType> {
        self.fields.get(name)
    }

    pub fn field_names(&self) -> Vec<String> {
        self.field_order.clone()
    }

    pub fn field_type_at(&self, index: usize) -> Option<&CometType> {
        self.field_order.get(index).and_then(|name| self.fields.get(name))
    }

    pub fn field_name_at(&self, index: usize) -> Option<&str> {
        self.field_order.get(index).map(String::as_str)
    }

    pub fn field_offset(&self, name: &str) -> Option<u32> {
        let mut offset = 0;

        for field_name in &self.field_order {
            let field = self.fields.get(field_name).unwrap();
            let alignment = field.cranelift_type.bytes().max(1);

            if alignment > 0 && offset % alignment != 0 {
                offset += alignment - (offset % alignment);
            }

            if field_name == name {
                return Some(offset);
            }

            offset += field.cranelift_type.bytes();
        }

        None
    }

    pub fn size(&self) -> u32 {
        let mut size = 0;

        for field_name in &self.field_order {
            let field = self.fields.get(field_name).unwrap();
            let alignment = field.cranelift_type.bytes().max(1);

            if alignment > 0 && size % alignment != 0 {
                size += alignment - (size % alignment);
            }

            size += field.cranelift_type.bytes();
        }

        let alignment = self.align();
        if size % alignment != 0 {
            size += alignment - (size % alignment);
        }

        size
    }

    pub fn align(&self) -> u32 {
        cmp::max(
            1,
            self.fields.values().map(|field| field.cranelift_type.bytes()).max().unwrap_or(1)
        )
    }

    pub fn payload_offset(&self, discriminant_size: u32) -> u32 {
        let alignment = self.align();

        if discriminant_size % alignment == 0 {
            discriminant_size
        } else {
            discriminant_size + alignment - (discriminant_size % alignment)
        }
    }
}