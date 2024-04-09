# Author:     Prasanna Paithankar
# Roll No.:   21CS30065
# Course:     Computer Networks Laboratory (CS39006) Spr 2023-24
# Date:       08/04/2024
# File:       Makefile

CC = gcc
CFLAGS = -Wall -Wextra -g -pedantic
DFLAGS = -DTEST

BINARIES = simDNSServer simDNSClient

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
LOG_DIR = logs
INC_DIR = include

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

.PHONY: all dir clean tar
	
all: $(BINARIES)

$(BINARIES): %: $(OBJ_DIR)/%.o
	make dir && $(CC) $(CFLAGS) $< -o $(BIN_DIR)/$@ $(DFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c 
	make dir && $(CC) $(CFLAGS) -c $< -o $@ $(DFLAGS)

dir:
	mkdir -p $(OBJ_DIR) $(BIN_DIR) $(LOG_DIR) 

tar:
	tar -czvf 21CS30065_Assignment6.tar.gz $(SRC_DIR) $(INC_DIR) Makefile README

clean:
	rm -rf $(BINARIES) $(OBJS) $(OBJ_DIR) $(BIN_DIR) 21CS30065_Assignment6.tar.gz
