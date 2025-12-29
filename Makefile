# Process-Sentinel Makefile

# Compiler and Flags
CC = gcc
CFLAGS = -Wall -Wextra -pthread -g
LDFLAGS = -pthread -lm

# Directories
SRC_DIR = .
DAEMON_DIR = $(SRC_DIR)/daemon
CLI_DIR = $(SRC_DIR)/cli
IPC_DIR = $(SRC_DIR)/ipc
INCLUDE_DIR = $(SRC_DIR)/include

# Source Files
DAEMON_SRCS = $(wildcard $(DAEMON_DIR)/*.c) $(wildcard $(IPC_DIR)/*.c)
CLI_SRCS = $(wildcard $(CLI_DIR)/*.c) $(wildcard $(IPC_DIR)/*.c)

# Object Files
DAEMON_OBJS = $(DAEMON_SRCS:.c=.o)
CLI_OBJS = $(CLI_SRCS:.c=.o)

# Targets
DAEMON_TARGET = sentinel_daemon
CLI_TARGET = sentinel_cli

# Default target
all: $(DAEMON_TARGET) $(CLI_TARGET)

# Rule to build the daemon
$(DAEMON_TARGET): $(DAEMON_OBJS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) $^ -o $@ $(LDFLAGS)

# Rule to build the CLI
$(CLI_TARGET): $(CLI_OBJS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) $^ -o $@ $(LDFLAGS)

# Generic rule for object files
%.o: %.c
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

# Phony targets
.PHONY: all clean run-daemon run-ai

# Clean up build artifacts
clean:
	rm -f $(DAEMON_TARGET) $(CLI_TARGET) $(DAEMON_OBJS) $(CLI_OBJS)

# Clean up IPC resources (helpful if daemon crashes)
ipc-clean:
	@echo "Cleaning up IPC resources..."
	-ipcs -m | grep 0x54322 | awk '{print $$2}' | xargs -r ipcrm -m
	-ipcs -s | grep 0x67891 | awk '{print $$2}' | xargs -r ipcrm -s
	-ipcs -q | grep 0x12346 | awk '{print $$2}' | xargs -r ipcrm -q

# Run targets
run-daemon: all
	@echo "Starting Sentinel Daemon..."
	./$(DAEMON_TARGET)

run-ai:
	@echo "Starting AI Agent..."
	@echo "NOTE: Make sure to set your HF_TOKEN environment variable."
	python3 ./ai/ai_agent.py

# A target to check for OS compatibility
check-os:
	@if [ "$(OS)" = "Windows_NT" ]; then \
		echo "ERROR: This project is designed for Linux and is not compatible with Windows."; \
		exit 1; \
	else \
		echo "OS check passed."; \
	fi
