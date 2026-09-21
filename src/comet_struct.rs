use std::cmp;
use std::collections::HashMap;

use crate::comet_type::CometType;

#[derive(Clone, Eq, PartialEq, Debug)]
pub struct CometStructField {
    name: String,
    field_type: CometType
}

#[derive(Clone, Eq, PartialEq, Debug)]
pub struct CometStruct {
    name: String,
    fields: Vec<CometStructField>,
    cached_fields: HashMap<String, usize>
}

impl CometStructField {
    pub fn new(name: String, field_type: CometType) -> Self {
        CometStructField { name: name, field_type: field_type }
    }
}

impl CometStruct {
    pub fn new(name: String, fields: Vec<CometStructField>) -> Self {
        let mut cached_fields = HashMap::new();

        for (idx, field) in fields.iter().enumerate() {
            cached_fields.insert(field.name.clone(), idx);
        }

        CometStruct { name: name, fields: fields, cached_fields: cached_fields }
    }

    pub fn name(&self) -> &str {
        return &self.name;
    }

    pub fn get_field_index(&self, name: &str) -> Option<&usize> {
        self.cached_fields.get(name)
    }

    /* returns field offsets and size of struct */
    pub fn get_layout(&self) -> (Vec<u32>, u32) {
        let mut current_offset = 0;
        let mut max_alignment = 0;
        let mut total_size = 0;

        let mut field_offsets = vec![0; self.fields.len()];

        for (i, field) in self.fields.iter().enumerate() {
            

            let align = field.field_type.cranelift_type.bytes();
            max_alignment = cmp::max(max_alignment, align);

            if current_offset % align != 0 {
                current_offset += align - (current_offset % align);
            }

            field_offsets[i] = current_offset;
            current_offset += align;

            
        }

        total_size = current_offset;
        if total_size % max_alignment != 0 {
            total_size += max_alignment - (total_size % max_alignment);
        }

        (field_offsets, total_size)
    }
}