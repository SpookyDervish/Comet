# Comet <img src="icon/icon.svg" width=25 alt="comet logo">

Comet is a fast, compiled programming built on top of C and LLVM. It has an enormous feature list, including:
- Classes (called structs)
- Templates
- Runtime exceptions
- Inline functions
- A system for casting values from one type to another
- An attribute system for templates
- Inheritance
- Imports and a package system (no more C-style header files!)
- An extensive standard library that handles memory management for you

## Completed Features
- [x] Variables
- [x] Loops
  - [x] While loops
  - [x] For loops
- [ ] Standard library
  - [ ] IO - Input/Output
    - [ ] Printing
    - [ ] File IO
  - [ ] Collections - data structures
    - [ ] List
    - [ ] Hashmap
  - [ ] Memory - manual memory management
  - [ ] Ansi - ANSI colour codes
  - [ ] String - string handling and management
- [ ] Imports
  - [ ] Package system
  - [ ] Package manager
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
    - [x] Constructor
    - [ ] Destructor
  - [x] "new" keyword
  - [x] Calling methods
  - [x] Inheritance
  - [ ] Templates
    - [ ] Template attributes
- [x] Command line args
- [ ] Exceptions
  - [ ] Throw exceptions
  - [ ] Catch exceptions

## Compiling
Just run `make` in the root of the repo. This will create the `cometc` executable. If you want a debug build, run `make debug`. Debug builds include the address sanitizer for tracking down segfaults. 