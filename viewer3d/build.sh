#!/bin/bash

echo "=== Compilation viewer3d ==="

CXX=g++
CXXFLAGS="-std=c++14 -O2 -Wno-deprecated-declarations"

INCLUDES="
-I/usr/local/include
-I/usr/include/SDL2
"

LIBS="
-L/usr/local/lib
-lMagnumSdl2Application
-lMagnumSceneGraph
-lMagnumTrade
-lMagnumPrimitives
-lMagnumShaders
-lMagnumMeshTools
-lMagnumGL
-lCorradeUtility
-lMagnum
-lSDL2
-lGL
"

echo "Compilation en cours..."
$CXX $CXXFLAGS $INCLUDES *.cpp -o viewer3d $LIBS

if [ $? -eq 0 ]; then
    echo "Compilation réussie."
    echo "Lance : ./viewer3d"
else
    echo "Échec de la compilation."
fi
