CC = gcc
CFLAGS = -Wall -Wextra -g -pedantic -O2

# Object files
OBJS_INIT = initmsocket.o msocket.o
OBJS = msocket.o user1.o user2.o initmsocket.o msocket.o

# Target executable
INIT_PROG = initmsocket
USER1_PROG = user1
USER2_PROG = user2

# Library file
LIBRARY = libmsocket.a

all: $(INIT_PROG) $(USER1_PROG) $(USER2_PROG) $(LIBRARY)

$(INIT_PROG): $(OBJS_INIT) $(LIBRARY)
	$(CC) $(CFLAGS) -o $@ $^ -L. -lmsocket -lpthread -lrt

$(USER1_PROG): user1.o $(LIBRARY)
	$(CC) $(CFLAGS) -o $@ $^ -L. -lmsocket -lpthread -lrt

$(USER2_PROG): user2.o $(LIBRARY)
	$(CC) $(CFLAGS) -o $@ $^ -L. -lmsocket -lpthread -lrt

$(LIBRARY): msocket.o
	ar rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJS) $(INIT_PROG) $(USER1_PROG) $(USER2_PROG) $(LIBRARY)
