import random
import time
from typing import List


from grid import (
   Grid,
   generer_grille_circulaire,
   afficher_core,
   afficher_grille,
   remplir_grille_symetrique,
)
from types_assemblage import TypeAssemblage
from thermique import (
   calculer_carte_thermique,
   diffusion_thermique,
   evaluer_thermique,
)




def palette() -> List[str]:
   """
   Palette de symboles pour l'affichage.


   Returns
   -------
   list of str
       Liste de symboles Unicode ou ASCII.
   """
   return ["🟦", "🟩", "🟥", "🟨", "🟧", "🟪", "🟫", "⬛", "⬜"]




# Variable globale pour la palette dynamique
types_global: List[TypeAssemblage] = []




def couleur_type(c: str) -> int:
   """
   Retourne l’index de couleur pour un symbole d’assemblage.


   Parameters
   ----------
   c : str
       Symbole d’assemblage.


   Returns
   -------
   int
       Index de couleur, -1 si inconnu.
   """
   mapping: dict[str, int] = {}
   # Génère dynamiquement le mapping selon les types saisis
   for idx, t in enumerate(types_global):
       mapping[t.symbole] = idx
   return mapping.get(c, -1)




def main() -> None:
   """
   Point d'entrée du programme.


   Workflow :
   - Saisie du rayon
   - Génération du cœur circulaire
   - Saisie des types d'assemblage (symbole + puissance)
   - Remplissage symétrique
   - Calcul thermique + diffusion
   - Affichage des indicateurs thermiques
   """
   random.seed(int(time.time()))


   # 1. Saisie du rayon et génération du cœur
   rayon = int(input("Rayon du cœur (par ex. 7) : "))
   G: Grid = generer_grille_circulaire(rayon)
   print("\n=== Cœur généré ===")
   afficher_core(G)


   # 2. Saisie interactive des types d'assemblages
   nb_types = int(input("Combien de types d'assemblages ? "))
   types: List[TypeAssemblage] = []
   for i in range(nb_types):
       symbole = input(f"Symbole du type {i + 1} : ")
       puissance = float(input(f"Puissance thermique du type {i + 1} : "))
       types.append(TypeAssemblage(symbole, puissance))


   global types_global
   types_global = types  # Pour la fonction couleur_type


   # 3. Remplissage symétrique
   remplir_grille_symetrique(G, types, nb_types)
   print("\n=== Remplissage symétrique ===")
   afficher_grille(G, palette(), couleur_type)


   # 4. Calcul thermique
   Tfield: list[list[float]] = [[0.0 for _ in range(G.size)] for _ in range(G.size)]
   calculer_carte_thermique(G, types, nb_types, Tfield)
   diffusion_thermique(G, Tfield, 5)


   # 5. Évaluation thermique (affiche Tmin, Tmax, ΔT, gradient max)
   evaluer_thermique(G, Tfield, 0.0, 0.0, 0.0, 0.0)




if __name__ == "__main__":
   main()

