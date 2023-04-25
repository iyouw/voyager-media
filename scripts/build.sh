#!/bin/bash -x

# verify emscripten version
emcc -v

# configure ffmpeg with emscripten
CFLAGS="-s USE_PTHREADS"
LDFLAGS="${CFLAGS} -s INITIAL_MEMORY=33554432" # 33554432 bytes = 32 MB
CONFIG_ARGS=(
    --target-os=none             # use none to prevent any os specific configurations
    --arch=x86_32                # use x86_32 to achieve minimal architectural optimization
    --enable-cross-compile       # enable cross compile
    --disable-x86asm             # disable x86 asm
    --disable-inline-asm         # disable inline asm
    --disable-stripping          # disable stripping
    --disable-programs           # disable programs build
    --disable-doc                # disable doc
    --extra-cflags="$CFLAGS"
    --extra-cxxflags="$CFLAGS"
    --extra-ldflags="$LDFLAGS"
    --nm="llvm-nm -g"
    --ar=emar
    --as=llvm-as
    --ranlib=llvm-ranlib
    --cc=emcc
    --cxx=em++
    --objcc=emcc
    --dep-cc=emcc
)

emconfigure ./configure "${CONFIG_ARGS[@]}"

# build dependencies
emmake make -j4

# build ffmpeg.wasm
mkdir -p wasm/dist

FFMPEG_SRC +=                   \
    fftools/ffmpeg_dec.c        \
    fftools/ffmpeg_demux.c      \
    fftools/ffmpeg_enc.c        \
    fftools/ffmpeg_filter.c     \
    fftools/ffmpeg_hw.c         \
    fftools/ffmpeg_mux.c        \
    fftools/ffmpeg_mux_init.c   \
    fftools/ffmpeg_opt.c        \
    fftools/objpool.c           \
    fftools/sync_queue.c        \
    fftools/thread_queue.c      \
    fftools/cmdutils.c          \
    fftools/opt_common.c        \
    fftools/ffmpeg.c

ARGS=(
    -I. -I./fftools
    -Llibavcodec
    -Llibavdevice
    -Llibavfilter
    -Llibavformat
    -Llibavresample
    -Llibavutil
    -Llibpostproc
    -Llibswscale
    -Llibswresample
    -Qunused-arguments
    -o wasm/dist/ffmpeg.js "${FFMPEG_SRC}"
    -l avdevice -lavfilter -lavformat -lavcodec -lswresample -lswscale -lavutil -lm
    -s USE_SDL=2                    # use SDL2
    -s USE_PTHREADS=1               # enable pthreads support
    -s INITIAL_MEMORY=33554432      # 33554432 bytes = 32 MB
)
emcc "${ARGS[@]}"


