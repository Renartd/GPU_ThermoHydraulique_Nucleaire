#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>
#include "../core/GridData.hpp"
#include "../core/ReactorParams.hpp"

class ThermalModel {
public:
    // Charge les températures depuis un CSV
    // Format CSV attendu :
    //   row,col,temperature
    //   0,0,295.3
    //   0,1,302.1
    //   ...
    static bool chargerCSV(const std::string& path, GridData& grid) {
        std::ifstream f(path);
        if (!f.is_open()) {
            std::cerr << "[ThermalModel] Fichier CSV introuvable : " << path << "\n";
            return false;
        }

        // Indexe les cubes par (row, col) pour accès rapide
        std::vector<std::vector<float>> tempMap(grid.rows,
            std::vector<float>(grid.cols, -1.0f));

        std::string line;
        std::getline(f, line); // skip header
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream ss(line);
            std::string tok;
            int row = -1, col = -1;
            float temp = 0.0f;
            try {
                std::getline(ss, tok, ','); row  = std::stoi(tok);
                std::getline(ss, tok, ','); col  = std::stoi(tok);
                std::getline(ss, tok, ','); temp = std::stof(tok);
            } catch(...) { continue; }

            if (row >= 0 && row < grid.rows &&
                col >= 0 && col < grid.cols)
                tempMap[row][col] = temp;
        }

        // Applique aux cubes
        int found = 0;
        for (auto& cube : grid.cubes) {
            float t = tempMap[cube.row][cube.col_idx];
            if (t >= 0.0f) { cube.temperature = t; ++found; }
        }
        std::cout << "[ThermalModel] " << found << "/" << grid.cubes.size()
                  << " températures chargées depuis " << path << "\n";
        grid.updateTempRange();
        return true;
    }

    // Profil gaussien simulé si pas de CSV disponible
    // (centre chaud, bords froids) — utile pour les tests
    static void simulerGaussien(GridData& grid, const ReactorParams& params) {
        float cx = 0.0f, cz = 0.0f;
        float maxDist = sqrtf(grid.offsetX * grid.offsetX +
                               grid.offsetZ * grid.offsetZ);
        if (maxDist < 0.001f) maxDist = 1.0f;

        float tRange = params.tempSortie - params.tempEntree;

        for (auto& cube : grid.cubes) {
            float dx   = cube.pos.x - cx;
            float dz   = cube.pos.z - cz;
            float dist = sqrtf(dx*dx + dz*dz);
            float sigma = maxDist * 0.5f;
            float gauss = expf(-(dist*dist) / (2.0f * sigma*sigma));
            cube.temperature = params.tempEntree + tRange * gauss;
        }
        grid.updateTempRange();
        std::cout << "[ThermalModel] Profil gaussien simulé.\n";
    }

    // Génère un CSV exemple pour la grille actuelle
    static void genererCSVExemple(const std::string& path,
                                   const GridData& grid,
                                   const ReactorParams& params) {
        std::ofstream f(path);
        f << "row,col,temperature\n";
        float cx = 0.0f, cz = 0.0f;
        float maxDist = sqrtf(grid.offsetX*grid.offsetX +
                               grid.offsetZ*grid.offsetZ);
        if (maxDist < 0.001f) maxDist = 1.0f;
        float tRange = params.tempSortie - params.tempEntree;

        for (const auto& cube : grid.cubes) {
            float dx   = cube.pos.x - cx;
            float dz   = cube.pos.z - cz;
            float dist = sqrtf(dx*dx + dz*dz);
            float sigma = maxDist * 0.5f;
            float gauss = expf(-(dist*dist) / (2.0f * sigma*sigma));
            float temp  = params.tempEntree + tRange * gauss;
            f << cube.row << "," << cube.col_idx << ","
              << std::fixed << temp << "\n";
        }
        std::cout << "[ThermalModel] CSV exemple généré : " << path << "\n";
    }
};
