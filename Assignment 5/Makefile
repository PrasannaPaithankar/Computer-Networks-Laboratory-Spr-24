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
LIB_DIR = lib
INC_DIR = include
LIB_OBJ_DIR = obj/lib

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
LIBSRC = $(wildcard $(INC_DIR)/*.c)
LIBOBJ = $(patsubst $(INC_DIR)/%.c,$(LIB_OBJ_DIR)/%.o,$(LIBSRC))

.PHONY: all dir clean tar run runkde rungnome
	
all: $(LIBRARY) $(BINARIES)

$(LIBRARY): $(LIBOBJ)
	make dir && ar rcs $(LIB_DIR)/$@ $^

$(BINARIES): %: $(OBJ_DIR)/%.o $(LIBRARY)
	make dir && $(CC) $(CFLAGS) $< -o $(BIN_DIR)/$@ -L$(LIB_DIR) $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c 
	make dir && $(CC) $(CFLAGS) -c $< -o $@ $(DFLAGS)

$(LIB_OBJ_DIR)/%.o: $(INC_DIR)/%.c
	make dir && $(CC) $(CFLAGS) -c $< -o $@ $(DFLAGS)

install: all
	cp $(LIB_DIR)/$(LIBRARY) /usr/local/lib
	cp $(INC_DIR)/*.h /usr/local/include
	ldconfig

uninstall:
	rm -f /usr/local/lib/$(LIBRARY)
	rm -f /usr/local/include/msocket.h
	ldconfig

dir:
	mkdir -p $(OBJ_DIR) $(BIN_DIR) $(LOG_DIR) $(LIB_DIR) $(INC_DIR) $(LIB_OBJ_DIR)

tar:
	tar -czvf assignment5.tar.gz $(SRC_DIR) $(INC_DIR) Makefile documentation.txt

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
	rm -rf $(LIBRARY) $(BINARIES) $(OBJS) $(LIBOBJ) $(LIB_DIR) $(OBJ_DIR) $(BIN_DIR) assignment5.tar.gz
