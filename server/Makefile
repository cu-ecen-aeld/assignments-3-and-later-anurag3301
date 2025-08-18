CROSS_COMPILE ?=
CC ?= $(CROSS_COMPILE)gcc

TARGET = aesdsocket

SRCS = main.c

OBJS = $(SRCS:.c=.o)

CFLAGS = -Wall -Werror -pthread -Wno-unused-result $(LDFLAGS)

ifeq ($(origin USE_AESD_CHAR_DEVICE), command line)
    CFLAGS += -DUSE_AESD_CHAR_DEVICE=$(USE_AESD_CHAR_DEVICE)
endif

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJS)
