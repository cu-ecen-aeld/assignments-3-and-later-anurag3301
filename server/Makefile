CROSS_COMPILE ?=
CC ?= $(CROSS_COMPILE)gcc

TARGET = aesdsocket

SRCS = main.c

OBJS = $(SRCS:.c=.o)

CFLAGS = -Wall -Werror -pthread

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJS)
