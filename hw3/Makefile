CC     = gcc
CFLAGS = -Wall -Wextra -O2
TARGET = myinit

all: $(TARGET)

$(TARGET): myinit.c
	$(CC) $(CFLAGS) -o $(TARGET) myinit.c

clean:
	rm -f $(TARGET) config1.txt config2.txt result.txt
