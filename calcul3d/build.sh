#!/bin/bash
echo "=== Build calcul3d ==="

echo "[1/2] Compilation shader RT..."
glslangValidator \
    -V \
    --target-env vulkan1.3 \
    compute/shaders/shadow_rq.comp \
    -o compute/shaders/shadow_rq.spv \
    && echo "      shadow_rq.spv OK" \
    || echo "      WARN : shader RT échoué — RT désactivé au runtime"

echo "[2/2] Compilation C++..."
g++ main.cpp \
    core/AssemblageLoader.cpp \
    -I. \
    -o viewer_raylib \
    -lraylib -lvulkan -lGL -lm -lpthread -ldl -lrt -lX11 \
    -std=c++17 \
    -O2

if [ $? -eq 0 ]; then
    echo "=== OK : ./viewer_raylib ==="
else
    echo "=== Echec compilation C++ ==="
fi
