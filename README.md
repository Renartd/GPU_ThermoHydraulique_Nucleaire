# Simulateur Neutronique‑Thermique 3D  
**Modélisation temps réel de réacteurs nucléaires avec accélération GPU Vulkan**

Ce dépôt contient un simulateur couplé neutronique‑thermique destiné à l’étude qualitative de transitoires rapides dans des réacteurs à neutrons thermiques et rapides.  
Le code implémente un solveur neutronique deux groupes, un modèle thermique tridimensionnel, la cinétique Xénon/Iode, ainsi qu’un pipeline de calcul massivement parallèle basé sur Vulkan.

Développé par **Eloi Kress**.

---

## Résumé

Ce travail présente un simulateur couplé neutronique‑thermique destiné à l’analyse de la génération et diffusion thermique des assemblages dans le réacteur. Le modèle neutronique repose sur une formulation de diffusion à deux groupes avec sections efficaces dépendantes de la température et prise en compte optionnelle de la cinétique Xénon/Iode. Le modèle thermique résout l’équation de conduction tridimensionnelle dans le combustible, avec un couplage simplifié au caloporteur permettant de représenter les rétroactions Doppler et thermiques. L’ensemble du calcul est accéléré par des shaders de calcul Vulkan exploitant le parallélisme massif des GPU, permettant une visualisation de configurations de cœur. Un solveur d’assemblage dédié génère les hétérogénéités spatiales utilisées comme conditions d’entrée du modèle 3D. L’outil prend en charge plusieurs filières de réacteurs : REP, CANDU, RBMK, réacteurs rapides au sodium ou au plomb‑bismuth, ainsi que les réacteurs à haute température. Ce travail n'est pas encore complet et est encore loin des réalités physiques. Il ne prend pas encore en compte les actions des opérateurs comme la gestion de barres de contrôle ou l'injection active d'éléments émetteurs de neutron ou neutrophage, par exemple.

---

## 1. Description scientifique

### 1.1 Modèle neutronique  
- Diffusion neutronique **deux groupes** (rapides / thermiques)  
- Sections efficaces dépendantes de la température  
- Conditions aux limites absorbantes ou réfléchissantes  
- Schéma numérique implicite stabilisé  
- Réduction GPU via compute shaders SPIR‑V

### 1.2 Modèle thermique  
- Équation de conduction 3D dans le combustible  
- Couplage fluide/caloporteur simplifié (convection paramétrée)  
- Rétroaction Doppler et dilatation thermique  
- Schéma explicite stabilisé sur grille régulière

### 1.3 Cinétique Xénon/Iode  
- Équations différentielles couplées  
- Prise en compte de l’empoisonnement spatial  
- Impact sur la réactivité locale et globale

### 1.4 Couplage multi‑physique  
Le couplage neutronique‑thermique‑cinétique est réalisé à chaque pas de temps via un orchestrateur séquentiel :

```
Solveur assemblage (C) → Assemblage.txt → Simulateur 3D (C++/Vulkan)
```

---

## 2. Documentation

La documentation complète (modèles physiques, schémas numériques, architecture GPU, guide utilisateur) est disponible dans :

```
calcul3d/documentation_Simulation_Nucleaire_Eloi_KRESS.docx
```

---

## 3. Installation

### 3.1 Dépendances

```bash
sudo apt install build-essential libraylib-dev libvulkan-dev \
                 vulkan-tools glslang-tools git gcc g++
```

### 3.2 Clonage

```bash
git clone https://github.com/Renartd/GPU_ThermoHydraulique_Nucleaire.git
cd GPU_ThermoHydraulique_Nucleaire
```

Le solveur d’assemblage est intégré directement dans le dépôt.

### 3.3 Compilation et exécution

```bash
make run
```

Ou séparément :

```bash
make build
./orchestrateur.sh
```

---

## 4. Structure du dépôt

```
GPU_ThermoHydraulique_Nucleaire/
│
├── calcul3d/                      Simulateur 3D (C++17, Vulkan, Raylib)
│   ├── compute/                   Pipelines GPU (SPIR‑V)
│   ├── physics/                   Modèles physiques
│   ├── render/                    Rendu 3D et interface
│   ├── core/                      Structures de données
│   └── data/                      Données générées
│       └── Assemblage.txt         Grille issue du solveur C
│
├── assemblage_solver/             Solveur d’assemblage (C)
│   └── nuclear/code_c/            Code source C
│
├── orchestrateur.sh               Chaînage solveur → simulateur
├── Makefile                       Compilation solveur + simulateur
└── README.md
```

---

## 5. Raccourcis clavier (interface scientifique)

| Touche | Fonction |
|--------|----------|
| S | Contrôle de la simulation |
| D | Résolution du maillage |
| P | Paramètres géométriques |
| R | Configuration du réacteur |
| X | Cinétique Xénon/Iode |
| C | Caloporteur |
| H | Plan de coupe |
| T | Colormap |
| G | Grille 3D |
| W | Mode filaire |
| F | Vecteurs caloporteur |

---

## 6. Réacteurs supportés

| Type | Combustible | Caloporteur | Modérateur |
|------|-------------|-------------|------------|
| REP 900 / 1300 MW | UO₂ / MOX | Eau H₂O | Eau H₂O |
| CANDU | UO₂ naturel | Eau lourde D₂O | Eau lourde D₂O |
| RNR‑Na | MOX rapide | Sodium | — |
| RNR‑Pb | MOX rapide | Plomb‑Bismuth | — |
| RHT / HTGR | UO₂ TRISO | Hélium | Graphite |

---

## 7. Mise à jour du solveur d’assemblage

Le solveur est intégré au dépôt.  
Pour mettre à jour son code :

```bash
cd assemblage_solver/nuclear/code_c
# modifications
cd ../../..
git add assemblage_solver
git commit -m "Mise à jour du solveur d'assemblage"
git push
```

---

## 8. Licence

Projet académique — Eloi Kress.

---