SRC=src

SRC_FILES=$(wildcard $(SRC)/*.c)

CC=gcc
CXX=g++
CFLAGS=-Wall -Wextra $(shell llvm-config --cflags)
LDFLAGS=$(shell llvm-config --ldflags)
LDLIBS=$(shell llvm-config --libs core) $(shell llvm-config --system-libs)
DEBUG_CFLAGS=-Wall -Wextra -ggdb $(shell llvm-config --cflags) -fsanitize=address -g

TARGET=cometc


$(TARGET): $(SRC_FILES)
	$(CC) $(SRC_FILES) -o $(TARGET) $(CFLAGS) $(LDFLAGS) $(LDLIBS)

debug:
	$(CC) $(SRC_FILES) -o $(TARGET) $(DEBUG_CFLAGS) $(LDFLAGS) $(LDLIBS)

clean:
	rm $(TARGET)