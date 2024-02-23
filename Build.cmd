@echo off
cd "%~dp0"
windres -i .rc -o .o
g++ -mwindows -municode -std=c++20 -static -Os -s WinMain.cpp .o -lOleAut32 -lOle32 -lRuntimeObject -lDwmapi -o CoreWindowDisplayMode.exe
del .o
upx --best --ultra-brute CoreWindowDisplayMode.exe>nul 2>&1