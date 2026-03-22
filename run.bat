@echo off
cmake --build build
if %errorlevel% equ 0 (
    echo.
    echo --- Ejecutando P2PChat ---
    .\build\P2PChat.exe
)
