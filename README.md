# Comet <img src="icon/icon.svg" width=25 alt="comet logo">

Comet is a fast, compiled programming language built in Rust using Cranelift. It has an enormous feature list, including:
- Classes (called structs)
- Generics
- Runtime exceptions
- Inline functions
- A system for casting values from one type to another
- A constraint system for generics (not done just yet!)
- Inheritance
- Imports and a package system (no more C-style header files!)
- An extensive standard library that handles memory management for you
- Arrays
- A package manager for external libraries (See [Nebula](https://chookspace.com/Comet/Nebula))

## Supported Systems
Any systems supported by the Cranelift backend can be compiled for. This includes Linux/OpenBSD, MacOS, Windows and any other operating system that runs on x86-64, ARM64, IBM-Z and RISC-V64.

## Completed Features
- [x] Variables
- [x] Loops
  - [x] While loops
  - [ ] For loops
- [ ] Standard library
  - [ ] IO - Input/Output
    - [ ] Printing
    - [ ] File IO
  - [ ] Collections - data structures
    - [ ] List
    - [ ] Hashmap
  - [ ] String - string handling and management
  - [ ] Sockets
    - [ ] IPv6
    - [ ] IPv4
    - [ ] Core features
- [ ] Imports
  - [ ] Package system
  - [ ] Package manager
- [x] Functions
  - [x] Returning
  - [ ] Inline functions
- [ ] Structs
  - [ ] Struct definition
    - [x] Fields
      - [ ] Private/protected/readonly
      - [ ] Default values
      - [x] Accessing / setting fields
    - [x] Methods
    - [ ] Special methods (as, etc...)
    - [x] Constructor
    - [ ] Destructor
  - [x] "new" keyword
  - [x] Calling methods
  - [ ] Inheritance
  - [ ] Generics
    - [ ] Base generics (creation, type checking, etc...)
    - [ ] Generic constraints
- [x] Command line args
- [ ] Arrays
  - [ ] Creation
  - [ ] Access
  - [ ] Changing values
- [ ] Exceptions
  - [ ] Throw exceptions
  - [ ] Catch exceptions
- [ ] Enums

## Syntax and Creating External Libs
- Syntax and tutorial: [Comet Website](https://chsp.au/Comet/Comet/docs.html)
- Creating an External Library: [Comet Wiki](https://chookspace.com/Comet/Comet/wiki)

## Installation
Comet has no official release yet, and thus you will have to build Comet from source.

### Compiling
You will need Rust installed on your system.  
  
Run `cargo install --path .` in the root of the repo. This will build the entire program and install Comet to your home cargo directory. Then, you can run the Comet compiler with the `Comet` command.
  
#### Getting Nebula
Nebula is on the Python package index, and can be installed with your favourite package manager (`pipx` is recommended)
```
pipx install nebpkg
```

But regular old `pip` works too:
```
pip install nebpkg
```

Then getting the standard library is as simple as running:
```
neb install stdlib
```