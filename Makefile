all:main

# VPATH variable
# VPATH = src

# vpath directive
vpath %.c src
vpath %.h src

# which compiler
CC = cc

# where are include files kept
INCLUDE = .:/usr/local/include

# options for dev
CFLAGS = -g -Wall

# options for release
# CFLAGS = -O -Wall -ansi

FFMPEG_LIBS=    libavdevice                        \
                libavformat                        \
                libavfilter                        \
                libavcodec                         \
                libswresample                      \
                libswscale                         \
                libavutil                          \

FLAGS += -Wall -g
FLAGS := $(shell pkg-config --cflags $(FFMPEG_LIBS)) $(FLAGS)
LDLIBS := $(shell pkg-config --libs $(FFMPEG_LIBS)) $(LDLIBS)

main: main.o memory_stream.o
	$(CC) -o main main.o memory_stream.o -lm -lpthread

main.o: main.c
	$(CC) -I$(INCLUDE) $(CFLAGS) -c $<

memory_stream.o: memory_stream.c  memory_stream.h
	$(CC) -I$(INCLUDE) $(CFLAGS) -c $<

demux_decode: demux_decode.c memory_stream.c memory_stream.h
	$(CC) -I$(INCLUDE) $(CFLAGS) -o demux_decode demux_decode.c memory_stream.c -L../lib -lavdevice -lavfilter -lavformat -lavcodec -lswresample -lswscale -lavutil -pthread -lm -latomic

clean:
	-rm -f main demux_decode *.o *.wasm