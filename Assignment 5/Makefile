CC = gcc
CFLAGS = -Wall -Wextra -g

# Object files
OBJS = msocket.o initmsocket.o

# Target executable
TARGET = initmsocket

# Library file
LIBRARY = libmsocket.a

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -L. -lmsocket

$(LIBRARY): msocket.o
	ar rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJS) $(TARGET) $(LIBRARY)
