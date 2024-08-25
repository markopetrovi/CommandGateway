# Default target
all: server

# Directories
SRC_DIR = src
INCLUDE_DIR = $(SRC_DIR)/include
BIN_DIR = bin

# Compiler and flags
CC = gcc
CFLAGS = -I$(INCLUDE_DIR)

# Server target
server: CFLAGS += -DSERVER_BUILD
server: $(BIN_DIR)/cgserver

# Client target
client: CFLAGS += -DCLIENT_BUILD
client: $(BIN_DIR)/cgclient

# Compile and link for server
$(BIN_DIR)/cgserver: $(wildcard $(SRC_DIR)/server/*.c) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

# Compile and link for client
$(BIN_DIR)/cgclient: $(wildcard $(SRC_DIR)/client/*.c) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

# Create binary directory if it doesn't exist
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Clean both server and client builds
clean:
	rm -f $(BIN_DIR)/*

# Phony targets
.PHONY: all clean cgserver cgclient
