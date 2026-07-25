@echo off

cl main.c /Iinclude /Isrc src/*.c src/windows/*.c /W4 /wd4100 /wd4702