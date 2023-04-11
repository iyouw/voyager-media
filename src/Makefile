all:main

# which compiler
CC = cc

# where are include files kept
INCLUDE = .

# options for dev
CFLAGS = -g -Wall -ansi

# options for release
# CFLAGS = -O -Wall -ansi

main:main.o memory_stream.o
	$(CC) -o main main.o memory_stream.o -lm

main.o:main.c
	$(CC) -I$(INCLUDE) $(CFLAGS) -c main.c

memory_stream.o:memory_stream.c
	$(CC) -I$(INCLUDE) $(CFLAGS) -c memory_stream.c

clean:
	-rm main *.o
