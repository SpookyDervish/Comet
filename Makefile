SRC=src

SRC_FILES=$(wildcard $(SRC)/*.c)

CC=gcc
CFLAGS=-Wall -pedantic -Wextra -O3
DEBUG_CFLAGS=-Wall -pedantic -Wextra -O0 -ggdb

TARGET=cometc


$(TARGET): $(SRC_FILES)
	$(CC) $(CFLAGS) $(SRC_FILES) -o $(TARGET)

debug:
	$(CC) $(DEBUG_CFLAGS) $(SRC_FILES) -o $(TARGET)

clean:
	rm $(TARGET)