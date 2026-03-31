# Simulateur Neutronique-Thermique 3D

Simulateur temps réel de réacteur nucléaire — diffusion neutronique 2 groupes, thermique 3D, cinétique Xénon/Iode, accélération GPU Vulkan.

Développé par **Eloi Kress**.

---

## Documentation & Guide utilisateur

La documentation complète du simulateur (description des modèles physiques, guide d'utilisation, architecture logicielle) est disponible dans :

```
calcul3d/documentation_Simulation_Nucleaire_Eloi_KRESS.docx
```

> Ce fichier contient :
> - Description des modèles physiques (neutronique 2G, thermique 3D, Xénon-Iode, précurseurs)
> - Guide utilisateur complet (panneaux de contrôle, raccourcis clavier)
> - Architecture GPU Vulkan et pipeline de calcul
> - Paramètres des réacteurs supportés (REP, CANDU, RNR-Na, RNR-Pb, RHT)

---

## Installation et lancement

### Prérequis

```bash
# Ubuntu / Debian
sudo apt install build-essential libraylib-dev libvulkan-dev \
                 vulkan-tools glslang-tools git gcc g++
```

### Cloner le dépôt (avec le solveur d'assemblage)

```bash
git clone --recurse-submodules https://github.com/Renartd/GPU_ThermoHydraulique_Nucleaire.git
cd GPU_ThermoHydraulique_Nucleaire
```

> Le `--recurse-submodules` est **obligatoire** pour récupérer le solveur d'assemblage.  
> Si vous avez oublié ce flag :
> ```bash
> git submodule update --init --recursive
> ```

### Compiler et lancer

```bash
# Tout en une commande
make run
```

Ou étape par étape :

```bash
make build    # compile le solveur C + le simulateur C++
./orchestrateur.sh   # lance le solveur puis le simulateur
```

---

## Structure du dépôt

```
GPU_ThermoHydraulique_Nucleaire/
│
├── calcul3d/                  ← Simulateur C++ (code principal)
│   ├── main.cpp
│   ├── build.sh
│   ├── compute/               ← Pipelines GPU Vulkan
│   │   └── shaders/           ← Shaders GLSL (diffusion.comp, neutron_fvm.comp)
│   ├── physics/               ← Modèles physiques (XS, Xénon, caloporteur)
│   ├── render/                ← Rendu Raylib 3D + panneaux UI
│   ├── core/                  ← Grille, maillage, paramètres
│   └── data/
│       └── Assemblage.txt     ← Grille générée par le solveur
│
├── assemblage_solver/         ← Solveur d'assemblage (submodule C)
│   └── ...                    ← Génère calcul3d/data/Assemblage.txt
│
├── orchestrateur.sh           ← Lance solveur → simulateur
├── Makefile                   ← Entrée principale
└── README.md
```

---

## Raccourcis clavier principaux

| Touche | Action |
|--------|--------|
| `S` | Panneau simulation (Start / Pause / Reset) |
| `D` | Résolution du maillage (sub_xy / sub_z) |
| `P` | Dimensions physiques des assemblages |
| `R` | Configuration réacteur (type, enrichissement) |
| `X` | Xénon-135 / Iode-135 (empoisonnement, bore) |
| `C` | Caloporteur (fluide, convection) |
| `H` | Plan de coupe 2D (heatmap) |
| `T` | Mode colormap température / type |
| `G` | Grille 3D |
| `W` | Filaires |
| `F` | Mode affichage flèches caloporteur |

---

## Réacteurs supportés

| Type | Combustible | Caloporteur | Modérateur |
|------|-------------|-------------|------------|
| REP 900 / 1300 MW | UO₂ / MOX | Eau H₂O | Eau H₂O |
| CANDU | UO₂ naturel | Eau lourde D₂O | Eau lourde D₂O |
| RNR-Na | MOX rapide | Sodium | — |
| RNR-Pb | MOX rapide | Plomb-Bismuth | — |
| RHT / HTGR | UO₂ TRISO | Hélium | Graphite |

---

## Dépendances

| Bibliothèque | Usage |
|---|---|
| [Raylib](https://www.raylib.com/) | Rendu 3D + interface utilisateur |
| [Vulkan](https://www.vulkan.org/) | Calcul GPU (thermique + neutronique) |
| GCC / G++ | Compilation C / C++17 |
| glslangValidator | Compilation shaders GLSL → SPIR-V |

---

## Mettre à jour le solveur d'assemblage

```bash
cd assemblage_solver
git pull origin master
cd ..
git add assemblage_solver
git commit -m "update assemblage_solver submodule"
git push
```

---

## Licence

Projet académique — Eloi Kress.
