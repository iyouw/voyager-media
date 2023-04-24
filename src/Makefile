all:main

# which compiler
CC = cc

# where are include files kept
INCLUDE = .:/usr/local/include

# options for dev
CFLAGS = -g -Wall

# options for release
# CFLAGS = -O -Wall -ansi

# FFMPEG LIBS from OS. use pkg-config to resolve relations
FFMPEG_LIBS += libavdevice
FFMPEG_LIBS += libavformat
FFMPEG_LIBS += libavfilter                        
FFMPEG_LIBS += libavcodec                         
FFMPEG_LIBS += libswresample                      
FFMPEG_LIBS += libswscale                         
FFMPEG_LIBS += libavutil                          

FLAGS += -Wall -g
FLAGS := $(shell pkg-config --cflags $(FFMPEG_LIBS)) $(FLAGS)
LDLIBS := $(shell pkg-config --libs $(FFMPEG_LIBS)) $(LDLIBS)

# FFMEPEG LIBS FROM lib
FLIBS += -L../lib
FLIBS += -lavdevice -lm -latomic
FLIBS += -lavfilter -pthread -lm -latomic
FLIBS += -lavformat -lm -latomic
FLIBS += -lavcodec -pthread -lm -latomic
FLIBS += -lswresample -lm -latomic
FLIBS += -lswscale -lm -latomic
FLIBS += -lavutil -pthread -lm -latomic

main: main.o memory_stream.o
	$(CC) -o $@ $^ -lm -lpthread

main.o: main.c
	$(CC) -I$(INCLUDE) $(CFLAGS) -c $<

memory_stream.o: memory_stream.c  memory_stream.h
	$(CC) -I$(INCLUDE) $(CFLAGS) -c $<

avio: avio.c memory_stream.c memory_stream.h
	$(CC) -I$(INCLUDE) $(CFLAGS) -o $@ avio.c memory_stream.c $(FLIBS)

# avio: avio.c memory_stream.c memory_stream.h
# 	$(CC) -I$(INCLUDE) $(FLAGS) -o $@ avio.c memory_stream.c $(LDLIBS)

demux_decode: demux_decode.c
	$(CC) -I$(INCLUDE) $(CFLAGS) -o $@ $^ $(FLIBS)

clean:
	-rm -f main avio demux_decode *.o *.wasm
