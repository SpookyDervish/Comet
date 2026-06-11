# Comet <img src="icon/icon.svg" width=25 alt="comet logo">

Comet is a fast, compiled programming language built on top of C and runs on a custom made stack-based VM. It has an enormous feature list, including:
- Classes (called structs)
- Templates
- Runtime exceptions
- Inline functions
- A system for casting values from one type to another
- An attribute system for templates
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
      - [ ] Private/protected/readonly
      - [ ] Default values
      - [x] Accessing / setting fields
    - [x] Methods
    - [x] Constructor
    - [ ] Destructor
  - [x] "new" keyword
  - [x] Calling methods
  - [ ] Inheritance
  - [ ] Templates
    - [ ] Template attributes
- [x] Command line args
- [ ] Arrays
  - [x] Creation
  - [x] Access
  - [ ] Changing values
- [ ] Exceptions
  - [ ] Throw exceptions
  - [ ] Catch exceptions

## Compiling
Just run `make` in the root of the repo. This will create the `cometc` and `comet` executables. If you want a debug build, run `make debug`. Debug builds include the address sanitizer for tracking down segfaults. 