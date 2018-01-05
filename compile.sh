#!bash
g++ -Wfatal-errors --std=c++17 -Wall -Wextra -pedantic -Wno-unused-function -Wno-unused-parameter main.cpp ocr.cpp depends/gl3w.c depends/libglfw3.a depends/libfreetype.a depends/libharfbuzz.a -o nezuyomi.exe -Iinclude -lopengl32 -lgdi32 -static -ggdb -mconsole -mwindows -municode -Os -fdata-sections -ffunction-sections -Wl,--gc-sections -Wl,--strip-all
