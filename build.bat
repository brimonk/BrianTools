@echo off

clang -g3 -o BrianTool.exe *.c -L E:\dev\lib -lsdl2 -luser32 -lsynchronization
