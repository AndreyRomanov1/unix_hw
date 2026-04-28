CC     = gcc
CFLAGS = -Wall -Wextra -O2
TARGET = mylock

all: $(TARGET)

$(TARGET): mylock.c
	$(CC) $(CFLAGS) -o $(TARGET) mylock.c

clean:
	rm -f $(TARGET) shared_file shared_file.lck stats.txt result.txt
