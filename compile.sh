#!bash
g++ --std=c++17 -Wall -Wextra -pedantic -Wno-unused-function -Wno-unused-parameter main.cpp ocr.cpp depends/gl3w.c depends/libglfw3.a -o nezuyomi.exe -Iinclude -lopengl32 -lgdi32 -static -lstdc++fs -ggdb -mconsole -mwindows -municode -Os -fdata-sections -ffunction-sections -Wl,--gc-sections -Wl,--strip-all
