@echo off
echo [*] Verificando compilador anterior...
if exist build\CMakeCache.txt (
    findstr /C:"MinGW Makefiles" build\CMakeCache.txt >nul
    if errorlevel 1 (
        echo [!] Detectada cache de Visual Studio. Limpiando para usar MinGW64...
        rmdir /s /q build
    )
)

echo [*] Configurando CMake con MinGW64...
cmake -G "MinGW Makefiles" -S . -B build

echo [*] Compilando el binario...
cmake --build build

if %errorlevel% equ 0 (
    echo.
    echo --- Ejecutando P2PChat ---
    .\build\P2PChat.exe
) else (
    echo.
    echo [ERROR] Falla en la compilacion. Revisa los errores arriba.
    pause
)
