@echo off
cmake -S . -B build
cmake --build build
if %errorlevel% equ 0 (
    echo.
    echo --- Ejecutando P2PChat ---
    .\build\P2PChat.exe
)
