CC = gcc
CFLAGS = -Wall -Wextra -O3
LDFLAGS = -lcwist -lttak -lcjson -lssl -lcrypto -luriparser -lpthread -ldl -lm

SRCS = src/main.c src/db.c src/handlers.c src/utils.c src/memory.c
OBJS = $(SRCS:.c=.o)
TARGET = server

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
