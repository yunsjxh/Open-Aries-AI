@echo off
g++ -O2 -std=c++17 mcp_server.cpp -o mcp_server.exe ^
    -ld3d11 -ldxgi -ldxguid -lwindowscodecs -lshlwapi -lole32 -luuid
if %errorlevel% equ 0 echo Build OK: mcp_server.exe
