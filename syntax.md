# Comet - Syntax
## Types
- small  | 8 bits
- int    | 32 bits
- big    | 64 bits
- float  | 32 bits
- double | 64 bits
- bool   | 1 bit
- void   | 0 bits
- string | (struct, any size)

## Comments
```lua
-- single line comment

//
multi
line
comment
\\
```

## Variables
```lua
small x = 123
int y = 123456
big z = 123123123123

float my_float = 3.1415
double hi = 0.123123123123

bool my_bool = true -- can also be false

bool MY_CONSTANT = true -- constants are all capitals, can not be changed
```

## Loops
```lua

-- the :do is a label showing what's being closed, these labels are optional (will give u a warning if u dont include it)
-- but MUST match what they're closing
while true {
    print("Hello, World!")
} :while

for i .. 10 {
    print(i)
} :for
```

## Functions
```lua
function my_function(small my_arg) {

} :my_function


my_function(123)
```

## Structs
```lua

-- private: read/write but only for the struct itself, nothing outside
-- protected: read only for anything outside the struct
struct Player {
    -- things can have default values or not
    protected string name

    protected small health = 100
    protected small age = 0

    function greet() {
        print("Hello, " + self.name + "!")
    } :greet

    constructor(string name) {
        self.name = name
    } :constructor
} :Player
```