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

# emcc

EMCC = emcc

EMCC_INCLUDE += -I.
EMCC_INCLUDE += -I/usr/local/include

# EMCC_CFALGS += -g
EMCC_CFALGS += -sMODULARIZE
# EMCC_CFALGS += -sALLOW_MEMORY_GROWTH
EMCC_CFALGS += -sALLOW_TABLE_GROWTH
EMCC_CFALGS += -s INITIAL_MEMORY=33554432
EMCC_CFALGS += -s USE_PTHREADS
EMCC_CFALGS += -sEXPORT_NAME=ffm
EMCC_CFALGS += -sEXPORTED_RUNTIME_METHODS=addFunction,UTF8ToString,stringToUTF8,writeArrayToMemory

EMCC_LDFLAGS += -L../lib_wasm
EMCC_LDFLAGS += -lavformat
EMCC_LDFLAGS += -lavcodec
EMCC_LDFLAGS += -lavutil
EMCC_LDFLAGS += -lswresample

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

avio_r: avio_r.c
	$(CC) -I$(INCLUDE) $(CFLAGS) -o $@ $^ $(FLIBS)

demux_decode: demux_decode.c
	$(CC) -I$(INCLUDE) $(CFLAGS) -o $@ $^ $(FLIBS)

demux_decode_p: demux_decode_p.c memory_stream.c memory_stream.h
	$(CC) -I$(INCLUDE) $(CFLAGS) -o $@ demux_decode_p.c memory_stream.c $(FLIBS)

transcode: transcode.c memory_stream.c memory_stream.h
	$(EMCC) $(EMCC_INCLUDE) $(EMCC_CFALGS) -o trancode.js transcode.c memory_stream.c $(EMCC_LDFLAGS)

clean:
	-rm -f main avio avio_r demux_decode demux_decode_p *.o *.wasm *.js
