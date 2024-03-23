# Author:     Prasanna Paithankar
# Roll No.:   21CS30065
# Course:     Computer Networks Laboratory (CS39006) Spr 2023-24
# Date:       15/03/2024
# File:       Makefile

CC = gcc
CFLAGS = -Wall -Wextra -g -pedantic
LDFLAGS = -lrt -pthread -lmsocket
DFLAGS = -DTEST -DDEEPLOG

LIBRARY = libmsocket.a
BINARIES = initmsocket user1 user2 tester

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
LOG_DIR = logs

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
LIBOBJ = $(OBJ_DIR)/msocket.o

.PHONY: all dir clean tar run runkde rungnome

all: $(LIBRARY) $(BINARIES)

$(LIBRARY): $(LIBOBJ)
	ar rcs $@ $^

$(BINARIES): %: $(OBJ_DIR)/%.o $(LIBRARY) dir
	$(CC) $(CFLAGS) $< -o $(BIN_DIR)/$@ -L. $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c dir
	$(CC) $(CFLAGS) -c $< -o $@ $(DFLAGS)

dir:
	mkdir -p $(OBJ_DIR) $(BIN_DIR) $(LOG_DIR)

tar:
	tar -czvf assignment5.tar.gz $(SRC_DIR) Makefile

run:
	@./$(BIN_DIR)/initmsocket &
	@./$(BIN_DIR)/user1 20000 20001 127.0.0.1 &
	@./$(BIN_DIR)/user2 20001 20000 127.0.0.1 &
	@echo "Running the programs"

runkde:
	@konsole --new-tab --hold -e ./$(BIN_DIR)/initmsocket &
	@konsole --new-tab --hold -e ./$(BIN_DIR)/user1 20000 20001 127.0.0.1 &
	@konsole --new-tab --hold -e ./$(BIN_DIR)/user2 20001 20000 127.0.0.1 &
	@echo "Running the programs"

rungnome:
	@gnome-terminal --tab --title="initmsocket" --command="bash -c './$(BIN_DIR)/initmsocket; exec bash'" \
	--tab --title="user1" --command="bash -c './$(BIN_DIR)/user1 20000 20001 127.0.0.1; exec bash'" \
	--tab --title="user2" --command="bash -c './$(BIN_DIR)/user2 20001 20000 127.0.0.1; exec bash" &
	@echo "Running the programs"
	
clean:
	rm -rf $(LIBRARY) $(BINARIES) $(OBJS) $(LIBOBJ) $(BIN_DIR)/* $(OBJ_DIR)/* assignment5.tar.gz
