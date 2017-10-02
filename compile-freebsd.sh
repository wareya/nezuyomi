#!/usr/bin/env bash
g++6 -Wfatal-errors --std=c++17 -Wall -Wextra -pedantic -Wno-unused-function -Wno-unused-parameter main.cpp ocr.cpp depends/gl3w.c -o nezuyomi $(pkg-config --cflags --libs glfw3) -Iinclude -Os -fdata-sections -ffunction-sections -Wl,--gc-sections -Wl,--strip-all
