#!bash
g++ --std=c++17 -Wall -Wextra -pedantic -Wno-unused-parameter main.cpp depends/gl3w.c depends/libglfw3.a -Iinclude -lopengl32 -lgdi32 -static -lstdc++fs -ggdb -mconsole -mwindows -municode -Os -fdata-sections -ffunction-sections -Wl,--gc-sections -Wl,--strip-all
