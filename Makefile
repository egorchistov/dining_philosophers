CFLAGS = -Wall -Wextra -pedantic -Wshadow -Wunused

.PHONY: all clean

all: main.c
	cc $(CFLAGS) main.c -o main

clean:
	rm main

