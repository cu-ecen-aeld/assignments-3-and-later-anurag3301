CROSS_COMPILE ?= 
CC := $(CROSS_COMPILE)gcc

# Target and sources
TARGET = writer
SRC = writer.c

# Compilation flags
CFLAGS = -Wall

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)
