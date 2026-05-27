CC = gcc
CFLAGS = -I. -Wall
TARGET = coo.exe

OBJS = main.o \
       core/commit.o \
       core/repo.o \
       core/tree.o \
       core/staging.o \
       core/repo_storage.o \
       utils.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

core/%.o: core/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean