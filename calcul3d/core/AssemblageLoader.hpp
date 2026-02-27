#pragma once
#include <string>
#include <vector>

struct Cell {
    bool isAssembly;   // true = assemblage, false = vide
    char symbol;       // symbole brut (A, Z, 1, X, etc.)
};

class AssemblageLoader {
public:
    static std::vector<std::vector<Cell>> load(const std::string& path);
};
