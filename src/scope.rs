use std::collections::HashMap;

pub struct Scope <'a, K, V> {
    local: HashMap<K, V>,
    parent: Option<&'a Scope<'a, K, V>>
}

impl <'a, K: std::hash::Hash + Eq, V> Scope<'a, K, V> {
    pub fn new(parent: Option<&'a Scope<'a, K, V>>) -> Self {
        Self {
            local: HashMap::new(),
            parent: parent
        }
    }

    pub fn insert(&mut self, key: K, value: V) {
        self.local.insert(key, value);
    }

    pub fn get(&self, key: &K) -> Option<&V> {
        self.local.get(key).or_else(|| {
            self.parent.and_then(|p| p.get(key))
        })
    }
}