#!/usr/bin/bash
LINKING_FLAGS=$(pkg-config --libs --cflags )
g++ ./src/main.cpp ${LINKING_FLAGS} -lm