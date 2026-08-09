@echo off

cl main.c -Zi /Iinclude /Isrc src/memmi.c /W4 /wd4100 /wd4702 /wd4127 /nologo
cl test.cpp -Zi /W4 /std:c++17 /nologo
