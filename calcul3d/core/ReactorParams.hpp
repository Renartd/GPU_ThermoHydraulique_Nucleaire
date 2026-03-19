#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <iostream>

struct ReactorParams {
    float enrichissement = 3.5f;
    float moderateur     = 1.0f;
    float puissance      = 100.0f;
    float tempEntree     = 286.0f;  // °C
    float tempSortie     = 324.0f;  // °C
    std::string reacteurStr    = "REP";
    std::string caloporteurStr = "EAU";

    static ReactorParams lireDepuisFichier(const std::string& path) {
        ReactorParams p;
        std::ifstream f(path);
        std::string line;
        while (std::getline(f, line)) {
            if      (line.rfind("# enrichissement=", 0) == 0) try { p.enrichissement = std::stof(line.substr(17)); } catch(...) {}
            else if (line.rfind("# moderateur=",     0) == 0) try { p.moderateur     = std::stof(line.substr(13)); } catch(...) {}
            else if (line.rfind("# puissance=",      0) == 0) try { p.puissance      = std::stof(line.substr(12)); } catch(...) {}
            else if (line.rfind("# tempEntree=",     0) == 0) try { p.tempEntree     = std::stof(line.substr(13)); } catch(...) {}
            else if (line.rfind("# tempSortie=",     0) == 0) try { p.tempSortie     = std::stof(line.substr(13)); } catch(...) {}
            else if (line.rfind("# reacteur=",       0) == 0) { p.reacteurStr   = line.substr(11); }
            else if (line.rfind("# caloporteur=",    0) == 0) { p.caloporteurStr = line.substr(14); }
        }
        return p;
    }

    void sauvegarder(const std::string& path) const {
        std::ifstream fin(path);
        std::vector<std::string> lignes;
        std::string l;
        while (std::getline(fin, l))
            if (!l.empty() && l[0] != '#') lignes.push_back(l);
        fin.close();
        std::ofstream fout(path);
        fout << "# enrichissement=" << enrichissement << "\n"
             << "# moderateur="     << moderateur     << "\n"
             << "# puissance="      << puissance      << "\n"
             << "# tempEntree="     << tempEntree     << "\n"
             << "# tempSortie="     << tempSortie     << "\n";
        for (auto& lg : lignes) fout << lg << "\n";
        std::cout << "[Config] Sauvegardé dans " << path << "\n";
    }

    void saisirConsole() {
        auto ask = [](const std::string& label, float def) -> float {
            std::cout << label << " [" << def << "] : ";
            std::string line; std::getline(std::cin, line);
            if (line.empty()) return def;
            try { return std::stof(line); } catch(...) { return def; }
        };
        std::cout << "\n=== Parametres Reacteur (Entree = valeur par defaut) ===\n\n";
        enrichissement = ask("Enrichissement uranium (%)",     enrichissement);
        moderateur     = ask("Ratio moderateur (1.0=nominal)", moderateur);
        puissance      = ask("Puissance reacteur (%)",         puissance);
        tempEntree     = ask("Temperature entree (C)",         tempEntree);
        tempSortie     = ask("Temperature sortie (C)",         tempSortie);
    }
};