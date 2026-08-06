@echo off

cl main.c -Zi /Iinclude /Isrc src/*.c src/windows/*.c /W4 /wd4100 /wd4702 /wd4127 /nologo
cl test.cpp -Zi /W4 /std:c++17 /nologo
