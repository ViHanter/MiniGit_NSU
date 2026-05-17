CC = gcc
CFLAGS = -I. -Wall
LDFLAGS =

# Для MSYS2 добавьте пути
# CFLAGS = -I. -I/ucrt64/include -Wall
# LDFLAGS = -L/ucrt64/lib -lssl -lcrypto

TARGET = minigit.exe
SRCS = core/commit.c core/repo.c core/tree.c core/staging.c utils.c main.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	-del /Q $(subst /,\,$(OBJS)) $(TARGET) 2>nul

.PHONY: all clean
