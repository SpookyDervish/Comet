# Comet <img src="icon/icon.svg" width=25 alt="comet logo">

Comet is a fast, compiled programming language built on top of C and runs on a custom made stack-based VM. It has an enormous feature list, including:
- Classes (called structs)
- Generics
- Runtime exceptions
- Inline functions
- A system for casting values from one type to another
- A constraint system for generics
- Inheritance
- Imports and a package system (no more C-style header files!)
- An extensive standard library that handles memory management for you
- Arrays

## Completed Features
- [x] Variables
- [x] Loops
  - [x] While loops
  - [x] For loops
- [ ] Standard library
  - [x] IO - Input/Output
    - [x] Printing
    - [x] File IO
  - [ ] Collections - data structures
    - [ ] List
    - [ ] Hashmap
  - [ ] String - string handling and management
- [x] Imports
  - [x] Package system
  - [x] Package manager
- [x] Functions
  - [x] Returning
  - [x] Inline functions
- [ ] Structs
  - [ ] Struct definition
    - [x] Fields
      - [x] Private/protected/readonly
      - [x] Default values
      - [x] Accessing / setting fields
    - [x] Methods
    - [ ] Special methods (as, etc...)
    - [x] Constructor
    - [x] Destructor
  - [x] "new" keyword
  - [x] Calling methods
  - [x] Inheritance
  - [ ] Generics
    - [x] Base generics (creation, type checking, etc...)
    - [ ] Generic constraints
- [x] Command line args
- [x] Arrays
  - [x] Creation
  - [x] Access
  - [x] Changing values
- [x] Exceptions
  - [x] Throw exceptions
  - [x] Catch exceptions
- [x] Enums

## Compiling
Just run `make install` in the root of the repo. This will install the `cometc` and `comet` executables as well as the core library needed to run comet. If you want a debug build, run `make debug` which will just put the `cometc` and `comet` executables in the root of the repo instead. Debug builds include the address sanitizer for tracking down segfaults. 