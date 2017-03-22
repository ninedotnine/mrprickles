CC = gcc
CFLAGS = -std=gnu99 -Wall -Wextra -g -pedantic
FILES = src/mrprickles.c
OUT_EXE = bin/mrprickles
LIBS = -lpthread -lsodium -ltoxcore -ltoxav

build:
	mkdir -p bin
	$(CC) $(CFLAGS) $(INCLUDES) -o $(OUT_EXE) $(FILES) $(LIBS)

clean:
	rm -f $(OUT_EXE)
