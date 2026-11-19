#!/bin/bash
# AI Studio: instala el plugin con un solo paso.
set -e
cd "$(dirname "$0")"

echo "== AI Studio: instalación =="

# 1. Herramientas de línea de comandos de Apple (incluye compilador y git)
if ! xcode-select -p >/dev/null 2>&1; then
    echo "Instalando las herramientas de Apple. Acepta la ventana que aparece."
    xcode-select --install || true
    echo "Cuando termine esa instalación, vuelve a ejecutar este archivo."
    exit 1
fi

# 2. CMake
if ! command -v cmake >/dev/null 2>&1; then
    if command -v brew >/dev/null 2>&1; then
        echo "Instalando CMake con Homebrew..."
        brew install cmake
    else
        echo "Falta CMake. Descárgalo de https://cmake.org/download/ (instalador para macOS),"
        echo "ábrelo una vez, y vuelve a ejecutar este archivo."
        exit 1
    fi
fi

# 3. Compilar (la primera vez descarga JUCE y tarda unos minutos)
echo "Compilando... (puede tardar 5-10 minutos la primera vez)"
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j "$(sysctl -n hw.ncpu)"

# 4. Refrescar el registro de plugins de macOS y validar
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true
echo "Validando el plugin..."
if auval -v aumu Aibm Aist 2>&1 | tail -n 3 | grep -q "SUCCEEDED"; then
    echo "Plugin validado correctamente."
else
    echo "Aviso: la validación no terminó bien. Copia el texto de arriba y envíamelo."
fi

echo
echo "LISTO. Abre tu programa y busca el instrumento 'AI Studio'"
echo "(en Logic Pro: Software Instrument > AU Instruments > AIStudio)."
echo "Si no aparece, reinicia el programa o haz un 'Rescan' de plugins."
read -n 1 -s -r -p "Pulsa cualquier tecla para cerrar..."
