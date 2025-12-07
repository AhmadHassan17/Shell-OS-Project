CC      := gcc
CFLAGS  := -Wall -Wextra -Werror -std=c11 -g
LDFLAGS :=

SRC_DIR := src
BIN_DIR := bin

SRCS := $(SRC_DIR)/main.c \
        $(SRC_DIR)/parser.c \
        $(SRC_DIR)/builtins.c \
        $(SRC_DIR)/exec.c \
        $(SRC_DIR)/jobs.c \
        $(SRC_DIR)/signals.c \
        $(SRC_DIR)/util.c \
        $(SRC_DIR)/loader.c \
        $(SRC_DIR)/loader_trampoline.S \
        $(SRC_DIR)/history.c \
        $(SRC_DIR)/aliases.c \
        $(SRC_DIR)/input.c \
        $(SRC_DIR)/completion.c \
        $(SRC_DIR)/glob.c

OBJS := $(SRCS:.c=.o)
OBJS := $(OBJS:.S=.o)

TARGET := $(BIN_DIR)/minishell_noexec

.PHONY: all clean

.INTERMEDIATE: $(OBJS)

all: $(TARGET)

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -f  $(TARGET)



