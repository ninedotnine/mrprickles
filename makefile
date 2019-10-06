CC = gcc
CFLAGS = -std=c11 -D _GNU_SOURCE -Wall -Wextra -g -pedantic -march=native -O2 -fmax-errors=3
FILES = src/*.c
OUT_EXE = bin/mrprickles
LIBS = -lpthread -lsodium -ltoxcore
# DEBUGFLAGS = -fsanitize=thread -fsanitize=undefined -fstack-protector-all
DEBUGFLAGS = -fsanitize=address -fsanitize=undefined -fstack-protector-all

build:
	mkdir -p bin
	$(CC) $(CFLAGS) -o $(OUT_EXE) $(FILES) $(LIBS)

clean:
	rm -f $(OUT_EXE)
