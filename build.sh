#!/bin/sh

rm -f src/main

cc -o src/main -g src/main.c src/memory_stream.c -lm