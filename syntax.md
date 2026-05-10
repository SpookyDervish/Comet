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

--[[
multi
line
comment
]]
```

## Variables
```lua
small x = 123
int y = 123456
big z = 123123123123

float my_float = 3.1415
double hi = 0.123123123123

bool my_bool = true -- can also be false

bool mut my_bool2 = true -- values must have the "mut" keyword to be able to be changed
```

## If-Statements
```lua
-- the :if is a label showing what's being closed, these labels are optional (will give u a warning if u dont include it)
-- but MUST match what they're closing

if 1+1 == 2 {
    print("Math is working!")
} :if
```

## Loops
```lua

while true {
    print("Hello, World!")
} :while

-- this is inclusive (will print numbers 1 to 10)
for int i in 1 .. 10 {
    if i == 6 {
        continue -- skip 6
    } :if

    if i == 8 {
        break -- end the loop at 7
    }

    print(i)

    
} :for
```

## Functions
```cpp
func add(int num1, int num2) -> int {
    return num1 + num2
} :add

-- functions can have an expression as a body
func square(int x) -> int => x * x 


int result = add(5, 2)
print(result)
```


## Structs
```lua

-- private: read/write but only for the struct itself, nothing outside
-- readonly: read only for everything outside, read/write for the struct itself
-- protected: read/write for struct and its substructs
-- public (default): read/write for everyone
struct Player {
    -- things can have default values or not
    readonly string name

    readonly small health = 100
    readonly small age = 0

    private int socialSecurityNumber

    func greet() -> void {
        print("Hello, " + self.name + "!")
    } :greet

    init(string name) {
        self.name = name
        self.socialSecurityNumber = 123456789
    } :init
} :Player


player1 = new Player("Player1")
```

## Templates
```lua
struct List <T: Any> {
    T myField

    func append(T value) -> void {
        -- ...
    }

    init(T in) {
        self.myField = in
    } :init
} :List

struct Hashmap <T: can be Hash> { -- can the given type be casted to Hash?
    
}

struct Example <T: can func speak() -> void> {
}

struct Example2 <T: Number> { -- is numeric, we can also say Float, Int, or a specific type name like List or small

}
```

## Inheritance
```lua
struct Animal {
    func speak() -> void {
        print("...")
    }

    init() {} : init
}

struct Dog : Animal {
    readonly string ownerName

    override func speak() -> void {
        print("Woof! My owner is " + self.ownerName as string)
    }

    init(string ownerName) {
        super()
        self.ownerName = ownerName

          
    } : init
}
```

## Enums
```lua
enum Colour : int {
    Red,
    Green,
    Blue
} :Colour

print(Colour.Red) -- prints 0
```

## Modules
```lua
import collections -- imports everything from collections

List<int> my_list = new List<int>()
```

## Casting
```lua
struct MyStruct {    
    readonly test = 123

    init() {} :init

    to string {
        return "<MyStruct test=" + self.test to string + ">"
    }
} :MyStruct

MyStruct new_struct = new MyStruct()
print(new_struct to string) -- "<MyStruct test=123>"
```

## Exceptions
### Catching Exceptions:
```lua
try {
    float x = 10 / 0
} except (ZeroDivisionError) {
    print("Attempt to divide by zero!")
}
```

### Throwing Exceptions:
```lua
func foo() -> void {
    throw new OutOfBoundsError("epic fail")
}

func main() -> int {
    foo() -- error!
    return 0
}
```

### Creating Exceptions:
```lua
struct MyError : BaseError {
}

func foo() -> void {
    throw new MyError("some message")
}
```