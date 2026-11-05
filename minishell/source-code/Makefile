CC = gcc

CFLAGS = -Wall -Wextra -Wpedantic -g -Iinclude

SRC = $(wildcard src/*.c)

TARGET = mini-shell

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean