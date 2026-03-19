#include "AssemblageLoader.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

std::vector<std::vector<Cell>> AssemblageLoader::load(const std::string& path) {
    std::ifstream file(path);
    std::vector<std::vector<Cell>> grid;

    if (!file.is_open()) {
        std::cerr << "[AssemblageLoader] Impossible d'ouvrir : " << path << "\n";
        return grid;
    }

    std::string line;
    bool in_grid = false;   // true entre "# Grille d'assemblage" et la suite
    int  size    = 0;       // taille lue depuis la première ligne numérique
    int  rows_read = 0;

    while (std::getline(file, line)) {

        // Commentaire → détecter le marqueur de début de grille
        if (!line.empty() && line[0] == '#') {
            if (line.find("Grille") != std::string::npos)
                in_grid = true;
            else
                in_grid = false;   // on sort si on tombe sur "# Types..."
            continue;
        }

        // Ligne vide → fin de section
        if (line.empty()) {
            in_grid = false;
            continue;
        }

        if (!in_grid) continue;

        // Première ligne numérique = taille de la grille
        if (size == 0) {
            try { size = std::stoi(line); } catch(...) {}
            continue;
        }

        // Lignes de la grille
        if (rows_read >= size) continue;

        std::vector<Cell> row;
        for (size_t i = 0; i < line.size(); ++i) {
            char c = line[i];
            if (c == ' ') continue;
            if (c == '-') { row.push_back({false, '-'}); continue; }
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                (c >= '0' && c <= '9'))
                row.push_back({true, c});
        }
        if (!row.empty()) {
            grid.push_back(row);
            rows_read++;
        }
    }

    std::cout << "[Loader] " << grid.size() << " lignes chargées\n";
    return grid;
}