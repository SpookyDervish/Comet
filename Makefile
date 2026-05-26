SRC=src
VM_SRC=vm_src

SRC_FILES=$(wildcard $(SRC)/*.c)
VM_SRC_FILES=$(wildcard $(VM_SRC)/*.c)

CC=gcc
CXX=g++
CFLAGS=-Wall -Wextra $(shell llvm-config --cflags) -I/Users/nathaniel.chandler/.brew/opt/uthash/include
LDFLAGS=$(shell llvm-config --ldflags)
LDLIBS=$(shell llvm-config --libs all) $(shell llvm-config --system-libs)
DEBUG_CFLAGS=-Wall -Wextra -ggdb $(shell llvm-config --cflags) -fsanitize=address -g

COMPILER_TARGET=cometc
VM_TARGET=comet

both: $(COMPILER_TARGET) $(VM_TARGET)

$(COMPILER_TARGET): $(SRC_FILES)
	$(CC) $(SRC_FILES) -o $(COMPILER_TARGET) $(CFLAGS) $(LDFLAGS) $(LDLIBS)

$(VM_TARGET): $(VM_SRC_FILES)
	$(CC) $(VM_SRC_FILES) -o $(VM_TARGET) $(CFLAGS) $(LDFLAGS) $(LDLIBS)

debug:
	$(CC) $(SRC_FILES) -o $(COMPILER_TARGET) $(DEBUG_CFLAGS) $(LDFLAGS) $(LDLIBS)

clean:
	rm $(COMPILER_TARGET)
