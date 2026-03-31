from dataclasses import dataclass
from typing import List

@dataclass
class TypeAssemblage:
    """
    Représente un type d’assemblage.

    Attributes
    ----------
    symbole : str
        Symbole unique représentant l’assemblage (ex: 'A', 'B', ...).
    conductivite : float
        Conductivité thermique de l’assemblage.
    """
    symbole: str
    conductivite: float

def definir_types(types: List[TypeAssemblage], nb_types: 'int') -> None:
    """
    Définit les types d’assemblages disponibles.

    Parameters
    ----------
    types : List[TypeAssemblage]
        Liste à remplir avec les types d’assemblages.
    nb_types : int
        Variable à remplir avec le nombre de types.

    Returns
    -------
    None
    """
    types.clear()
    types.append(TypeAssemblage(symbole='A', conductivite=1.0))
    types.append(TypeAssemblage(symbole='B', conductivite=0.7))
    types.append(TypeAssemblage(symbole='C', conductivite=0.3))
    nb_types = len(types)
    return nb_types
