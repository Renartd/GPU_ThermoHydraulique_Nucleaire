#include "AssemblageLoader.hpp"
#include <fstream>
#include <iostream>

std::vector<std::vector<Cell>> AssemblageLoader::load(const std::string& path) {
    std::ifstream file(path);
    std::vector<std::vector<Cell>> grid;

    if (!file.is_open()) {
        std::cerr << "[AssemblageLoader] Impossible d'ouvrir : " << path << "\n";
        return grid;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Ignorer commentaires et lignes vides
        if (line.empty() || line[0] == '#') continue;

        std::vector<Cell> row;
        for (size_t i = 0; i < line.size(); ++i) {
            char c = line[i];
            if (c == ' ') continue;
            if (c == '-') { row.push_back({false, '-'}); continue; }
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                (c >= '0' && c <= '9'))
                row.push_back({true, c});
        }
        if (!row.empty()) grid.push_back(row);
    }

    std::cout << "[Loader] " << grid.size() << " lignes chargées\n";
    return grid;
}
