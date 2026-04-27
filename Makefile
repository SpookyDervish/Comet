SRC=src

SRC_FILES=$(wildcard $(SRC)/*.c)

CC=gcc
CFLAGS=-Wall -pedantic -Wextra -O3

TARGET=cometc


$(TARGET): $(SRC_FILES)
	$(CC) $(CFLAGS) $(SRC_FILES) -o $(TARGET)

clean:
	rm $(TARGET)