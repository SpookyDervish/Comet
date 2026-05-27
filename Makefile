SRC=src
VM_SRC=vm_src

SRC_FILES=$(wildcard $(SRC)/*.c)
VM_SRC_FILES=$(wildcard $(VM_SRC)/*.c)

CC=gcc
CXX=g++
CFLAGS=-Wall -Wextra  -Wno-trigraphs -I/Users/nathaniel.chandler/.brew/opt/uthash/include
LDFLAGS=
LDLIBS=
DEBUG_CFLAGS=-Wall -Wextra -ggdb -fsanitize=address -g

COMPILER_TARGET=cometc
VM_TARGET=comet

both: $(COMPILER_TARGET) $(VM_TARGET)

$(COMPILER_TARGET): $(SRC_FILES)
	$(CC) $(SRC_FILES) -o $(COMPILER_TARGET) $(CFLAGS) $(LDFLAGS) $(LDLIBS)

$(VM_TARGET): $(VM_SRC_FILES)
	$(CC) $(VM_SRC_FILES) -o $(VM_TARGET) $(CFLAGS) $(LDFLAGS) $(LDLIBS)

debug:
	$(CC) $(SRC_FILES) -o $(COMPILER_TARGET) $(DEBUG_CFLAGS) $(LDFLAGS) $(LDLIBS)
	$(CC) $(VM_SRC_FILES) -o $(VM_TARGET) $(DEBUG_CFLAGS) $(LDFLAGS) $(LDLIBS)

clean:
	rm $(VM_TARGET)
	rm $(COMPILER_TARGET)
	