#!/bin/bash

echo "Compilation..."
gcc -Wall -Wextra -O2 -o programme $(find . -name "*.c") -lm


if [ $? -ne 0 ]; then
    echo "Erreur de compilation."
    exit 1
fi

echo "Exécution :"
./programme
