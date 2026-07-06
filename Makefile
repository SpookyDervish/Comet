SRC=src
VM_SRC=vm_src
SHARED=shared
CORELIB=corelib

SRC_FILES=src/main.comet
VM_SRC_FILES=$(wildcard $(VM_SRC)/*.c)
SHARED_SRC_FILES=$(wildcard $(SHARED)/*.c)
CORELIB_FILES=$(wildcard $(CORELIB)/*.c)

CC=gcc
COMETC=cometc
CFLAGS=-Wall -Wextra -Wno-trigraphs -O3 -rdynamic
LDFLAGS=
LDLIBS=-lm
DEBUG_CFLAGS=-Wall -Wextra -Wno-trigraphs -ggdb -g -rdynamic -fsanitize=address 

COMPILER_TARGET=cometc
VM_TARGET=comet

both: $(COMPILER_TARGET) $(VM_TARGET)

$(COMPILER_TARGET): $(SRC_FILES)
	$(COMETC) $(SRC_FILES) -o $(COMPILER_TARGET)

$(VM_TARGET): $(VM_SRC_FILES) $(SHARED_SRC_FILES)
	$(CC) $(VM_SRC_FILES) $(SHARED_SRC_FILES) -o $(VM_TARGET) $(CFLAGS) $(LDFLAGS) $(LDLIBS)

debug:
	$(CC) $(SRC_FILES) $(SHARED_SRC_FILES) -o $(COMPILER_TARGET) $(DEBUG_CFLAGS) $(LDFLAGS) $(LDLIBS)
	$(CC) $(VM_SRC_FILES) $(SHARED_SRC_FILES) -o $(VM_TARGET) $(DEBUG_CFLAGS) $(LDFLAGS) $(LDLIBS)

clean:
	rm $(VM_TARGET)
	rm $(COMPILER_TARGET)