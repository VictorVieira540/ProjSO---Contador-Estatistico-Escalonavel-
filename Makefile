CC = gcc
CFLAGS = -O3 -Wall
LIBS = -lpthread

SRC_DIR = src
BIN_DIR = .
TARGET = $(BIN_DIR)/contador

all: $(TARGET)

$(TARGET): $(SRC_DIR)/main.c
	$(CC) $(CFLAGS) $(SRC_DIR)/main.c -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
