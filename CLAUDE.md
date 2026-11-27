# Contexte pour Claude

**À lire en premier : `docs/REPRISE.md`** (reprendre sans mémoire : où sont les choses, versions, ce qui est en cours), puis **`docs/JOURNAL.md`** (fil rouge du projet : état, décisions, découvertes,
pièges). À la fin de chaque séance, y ajouter une entrée datée et mettre à jour « État actuel ».
Commenter le code pour qu'une session sans mémoire comprenne le *pourquoi* : chaque crochet (adresse,
ce qu'il remplace, pourquoi), chaque adresse ou décalage de l'officiel (d'où il vient, comment il a
été vérifié).

Firmware personnel d'AvrekStudio pour le Polyend Play+ (patch du firmware officiel 1.3.0). Nos
fonctionnalités sont écrites de zéro. Les outils (build, vérificateur, Play+ virtuel, symboles)
viennent de FreePlay (MIT), mais on **n'implémente pas** les fonctionnalités de FreePlay.
Le propriétaire écrit en français : réponds et documente en français.
Objectifs : moteurs de son maison, macros sur les potards, animations EVE façon OP-1
(`docs/FEUILLE_DE_ROUTE.md`). Grand projet (LOOM + Play+) et connaissance de la machine : `docs/PLATEFORME.md`.
Guide de travail : `docs/GUIDE.md`.

## Règles

- Jamais de firmware Polyend dans git (`*.ppf`, `*.bin`, `*.hex`), jamais de `.ppf` patché publié,
  jamais de code ou de sortie de décompilateur copiés depuis le firmware officiel. Les adresses et les
  notes de comportement sont autorisées.
- On patche les octets des enregistrements HEX existants (`cfw/tools/ppf.py`), on ne réécrit jamais un `.ppf`.
- C freestanding : pas de libc, pas d'affectation de struct, pas de sections inscriptibles (état dans
  `cfw_ram`, 256 o à 0x20002700, ou sur le tas), pas de gros `switch`. Le chemin audio tourne sous
  interruption : rapide, sans allocation ni accès fichier.
- Le firmware officiel attendu est `PlayPlus_v1.3.0.ppf` à la racine
  (sha256 535a903dffea93b9e624c7d0f613823812f7f7b1df74c91e93e3e93e8de94876).
- Chaque crochet ajouté va dans `target.json` (adresse, valeur attendue, symbole) ; chaque fonction de
  l'officiel appelée va dans `symbols.ld` (vérifier d'abord que le nom n'y est pas déjà : la dernière
  définition gagne, sans erreur). Le build doit finir sur `Result: **PASS**`.

## Commandes

- `scripts/setup.sh` : environnement (clang, mtools, mkfs.fat, `.venv`).
- `scripts/test.sh [vdev_xxx ...]` : build (secondes) puis tests sur le Play+ virtuel. `NOBUILD=1` pour
  sauter le build. Premier démarrage d'une image : environ 10 min ; instantanés dans `~/.cache/vplay/`.
- Tout nouveau comportement a un test `cfw/tests/vdev_*.py` (modèle : `vdev_base.py`).

## Git

`main` toujours vert ; une branche `feat/...` par fonctionnalité, Pull Request vers `main` ;
la version (`CFW_VERSION_STRING` dans `target.h`) et une étiquette `vX.Y.Z` à chaque étape validée
sur la machine.
