CC      = gcc
CFLAGS  = -Wall -Wextra -O2
TARGET  = myprogram

.PHONY: all clean

all: $(TARGET)

$(TARGET): myprogram.c
	$(CC) $(CFLAGS) -o $(TARGET) myprogram.c

clean:
	rm -f $(TARGET) fileA fileA.gz fileB fileB.gz fileC fileD result.txt
