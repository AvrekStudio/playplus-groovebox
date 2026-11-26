# Correspondance FM-1 → Play+ : LOOM-8

Matrice du propriétaire, faite dans la page « Câblage FM-1 → Play+ » ; fichier source : `loom-8.json` (format `fm1-playplus-map/1`). Fiche générée par `guest/loom/gen_doc.py` : ne pas la modifier à la main.

## Comment la lire

- Grille 16×8 : colonnes c0 à c15 ; grille 4×8 : ses colonnes c0 à c3 (c16 à c19 au total) ; r0 = rangée du haut.
- Potards du Play+ : 1 à 5 = rangée du haut, 6 à 10 = milieu, 11 à 15 = bas.
- « cran + / cran − » : un appui = un cran de l’encodeur du FM-1 vers la droite / la gauche.
- « valeur N » : l’encodeur est amené à la position N (pour ALGORITHM : la piste N).
- Combinaisons : l’action du Play+ produit, côté firmware, HOME tenu + le contrôle nommé.
- Le Shift du Play+ (B10) est une couche à nous, jamais transmise au firmware.

## Boutons

| FM-1 | Play+ | Rôle dans LOOM-8 |
|---|---|---|
| OCT- | Grille 4×8 r0 c2 | Jeu : Octave − · Steps (SEQ) : Mesure précédente ; avec SEQ tenu : pattern une octave plus bas ; pas tenu : décale plus tôt · Pas à pas (REC) : Recule d’un pas |
| OCT+ | Grille 4×8 r0 c3 | Jeu : Octave + · Steps (SEQ) : Mesure suivante ; avec SEQ tenu : pattern une octave plus haut ; pas tenu : décale plus tard · Pas à pas (REC) : Saute un pas |
| PLAY | B7 | Lecture / arrêt ; SEQ + PLAY : part de la mesure à l’écran ; REC + PLAY : enregistrement en direct |
| SEQ | B9 | Jeu : Ouvre Steps (page 2 : Bar) · Steps (SEQ) : Tenu : les touches jouent la piste et choisissent ce qu’un pas écrit ; tenu sur Bar : noires 1 à 4 = mesures |
| REC | B8 | Jeu : Tenu en lecture : enregistre tant qu’on le tient · Steps (SEQ) : Tapé à l’arrêt : enregistrement pas à pas ; tenu à l’arrêt : réglages (click, décompte, quantize, mode) · Pas à pas (REC) : Tapé : arrête le pas à pas ; REC + OCT+ : efface et avance ; REC + OCT− : recule et efface |
| ARP | Grille 4×8 r7 c3 | Players (arp, strum, hold, random) ; pas tenu + ARP : ses tricks |
| FX | Grille 4×8 r6 c3 | Character, Sends, FX I et FX II, Throws |
| EDIT | Grille 4×8 r2 c3 | Engine : le son, puis comment il joue (Play, Play 2…) |
| LFO | Grille 4×8 r3 c3 | LFO (page 2 : 4e et 5e potards) |
| ENV | Grille 4×8 r4 c3 | Enveloppe, puis Filtre et son enveloppe |
| HOME | B1 Fill | Arrange, puis Song ; tenu = Shift de LOOM ; « non » aux questions |
| SAVE | B4 Save | Enregistre ; tenu + touche blanche : emplacement, ou garder un son |
| GLO | B2 Patterns | Mix, puis Master EQ, couleur, sortie, LFO commun, Setup |
| SEL | B5 Copy | Liste des réglages et actions de la page ; pas tenu + SEL : notes du pas ; « oui » aux questions |

## Encodeurs et MASTER

| FM-1 | Play+ | Rôle dans LOOM-8 |
|---|---|---|
| KNOB 3 | Potard 3 (rangée 1, « Sample ») | Jeu : Valeur 3 en bas de l’écran · Steps (SEQ) : Pas tenu : chance |
| KNOB 4 | Potard 4 (rangée 1, « Reverb Send ») | Jeu : Valeur 4 en bas de l’écran · Steps (SEQ) : Pas tenu : hauteur du pas |
| KNOB 1 | Potard 1 (rangée 1, « Master Volume ») | Jeu : Valeur 1 en bas de l’écran · Steps (SEQ) : Pas tenu : vélocité |
| KNOB 2 | Potard 2 (rangée 1, « Note ») | Jeu : Valeur 2 en bas de l’écran · Steps (SEQ) : Pas tenu : longueur |
| MASTER | Potard 5 (rangée 1, « Chance ») | Volume général (géré par la base fm1-base, pas par LOOM) |
| PRESETS | Encodeur (rotation) | Élément suivant : preset, son de batterie, effet, pattern, scène |
| SELECT | B3 — cran + | Page suivante du bouton ; monte et descend dans une liste |
| SELECT | B0 — cran − | Page suivante du bouton ; monte et descend dans une liste |
| ALGORITHM | Grille 4×8 r0 c0 — valeur 1 | Choix de la piste (T1–T8, COND, THROW, WEAVE) ; valeur de la ligne dans une liste |
| ALGORITHM | Grille 4×8 r1 c0 — valeur 2 | Choix de la piste (T1–T8, COND, THROW, WEAVE) ; valeur de la ligne dans une liste |
| ALGORITHM | Grille 4×8 r2 c0 — valeur 3 | Choix de la piste (T1–T8, COND, THROW, WEAVE) ; valeur de la ligne dans une liste |
| ALGORITHM | Grille 4×8 r3 c0 — valeur 4 | Choix de la piste (T1–T8, COND, THROW, WEAVE) ; valeur de la ligne dans une liste |
| ALGORITHM | Grille 4×8 r4 c0 — valeur 5 | Choix de la piste (T1–T8, COND, THROW, WEAVE) ; valeur de la ligne dans une liste |
| ALGORITHM | Grille 4×8 r5 c0 — valeur 6 | Choix de la piste (T1–T8, COND, THROW, WEAVE) ; valeur de la ligne dans une liste |
| ALGORITHM | Grille 4×8 r6 c0 — valeur 7 | Choix de la piste (T1–T8, COND, THROW, WEAVE) ; valeur de la ligne dans une liste |
| ALGORITHM | Grille 4×8 r7 c0 — valeur 8 | Choix de la piste (T1–T8, COND, THROW, WEAVE) ; valeur de la ligne dans une liste |

## Combinaisons (HOME tenu)

| FM-1 | Play+ | Rôle dans LOOM-8 |
|---|---|---|
| HOME + KNOB 1 (combinaison) | Shift + Potard 6 (rangée 2, « Tempo ») | Tempo en dixièmes (page HOME) |
| HOME + SELECT (combinaison) | Potard 6 (rangée 2, « Tempo ») | Tempo, sur toutes les pages |

## Clavier

| FM-1 | Play+ | Rôle dans LOOM-8 |
|---|---|---|
| blanche 1 (F3) | Grille 16×8 r5 c0 | Jeu : Notes (la piste choisie) · Steps (SEQ) : Pas 1 à 16 : tapé = pose ou efface, tenu = édite le pas · Pas à pas (REC) : Notes écrites au curseur rouge · Ailleurs : Avec SAVE tenu : emplacements ; Arrange : scènes |
| noire 1 (F#3) | Grille 16×8 r4 c1 | Jeu : Notes ; sur une piste batterie : les 11 sons · Steps (SEQ) : Batterie : choisit le son ; pas tenu + noire : ajoute ou retire le son · Ailleurs : Arrange et Mix : pistes 1 à 8 (allumée si la piste joue) |
| blanche 2 (G3) | Grille 16×8 r5 c1 | Jeu : Notes (la piste choisie) · Steps (SEQ) : Pas 1 à 16 : tapé = pose ou efface, tenu = édite le pas · Pas à pas (REC) : Notes écrites au curseur rouge · Ailleurs : Avec SAVE tenu : emplacements ; Arrange : scènes |
| noire 2 (G#3) | Grille 16×8 r4 c2 | Jeu : Notes ; sur une piste batterie : les 11 sons · Steps (SEQ) : Batterie : choisit le son ; pas tenu + noire : ajoute ou retire le son · Ailleurs : Arrange et Mix : pistes 1 à 8 (allumée si la piste joue) |
| blanche 3 (A3) | Grille 16×8 r5 c2 | Jeu : Notes (la piste choisie) · Steps (SEQ) : Pas 1 à 16 : tapé = pose ou efface, tenu = édite le pas · Pas à pas (REC) : Notes écrites au curseur rouge · Ailleurs : Avec SAVE tenu : emplacements ; Arrange : scènes |
| noire 3 (A#3) | Grille 16×8 r4 c3 | Jeu : Notes ; sur une piste batterie : les 11 sons · Steps (SEQ) : Batterie : choisit le son ; pas tenu + noire : ajoute ou retire le son · Ailleurs : Arrange et Mix : pistes 1 à 8 (allumée si la piste joue) |
| blanche 4 (B3) | Grille 16×8 r5 c3 | Jeu : Notes (la piste choisie) · Steps (SEQ) : Pas 1 à 16 : tapé = pose ou efface, tenu = édite le pas · Pas à pas (REC) : Notes écrites au curseur rouge · Ailleurs : Avec SAVE tenu : emplacements ; Arrange : scènes |
| blanche 5 (C4) | Grille 16×8 r5 c4 | Jeu : Notes (la piste choisie) · Steps (SEQ) : Pas 1 à 16 : tapé = pose ou efface, tenu = édite le pas · Pas à pas (REC) : Notes écrites au curseur rouge · Ailleurs : Avec SAVE tenu : emplacements ; Arrange : scènes |
| noire 4 (C#4) | Grille 16×8 r4 c5 | Jeu : Notes ; sur une piste batterie : les 11 sons · Steps (SEQ) : Batterie : choisit le son ; pas tenu + noire : ajoute ou retire le son · Ailleurs : Arrange et Mix : pistes 1 à 8 (allumée si la piste joue) |
| blanche 6 (D4) | Grille 16×8 r5 c5 | Jeu : Notes (la piste choisie) · Steps (SEQ) : Pas 1 à 16 : tapé = pose ou efface, tenu = édite le pas · Pas à pas (REC) : Notes écrites au curseur rouge · Ailleurs : Avec SAVE tenu : emplacements ; Arrange : scènes |
| noire 5 (D#4) | Grille 16×8 r4 c6 | Jeu : Notes ; sur une piste batterie : les 11 sons · Steps (SEQ) : Batterie : choisit le son ; pas tenu + noire : ajoute ou retire le son · Ailleurs : Arrange et Mix : pistes 1 à 8 (allumée si la piste joue) |
| blanche 7 (E4) | Grille 16×8 r5 c6 | Jeu : Notes (la piste choisie) · Steps (SEQ) : Pas 1 à 16 : tapé = pose ou efface, tenu = édite le pas · Pas à pas (REC) : Notes écrites au curseur rouge · Ailleurs : Avec SAVE tenu : emplacements ; Arrange : scènes |
| blanche 8 (F4) | Grille 16×8 r5 c7 | Jeu : Notes (la piste choisie) · Steps (SEQ) : Pas 1 à 16 : tapé = pose ou efface, tenu = édite le pas · Pas à pas (REC) : Notes écrites au curseur rouge · Ailleurs : Avec SAVE tenu : emplacements ; Arrange : scènes |
| noire 6 (F#4) | Grille 16×8 r4 c8 | Jeu : Notes ; sur une piste batterie : les 11 sons · Steps (SEQ) : Batterie : choisit le son ; pas tenu + noire : ajoute ou retire le son · Ailleurs : Arrange et Mix : pistes 1 à 8 (allumée si la piste joue) |
| blanche 9 (G4) | Grille 16×8 r5 c8 | Jeu : Notes (la piste choisie) · Steps (SEQ) : Pas 1 à 16 : tapé = pose ou efface, tenu = édite le pas · Pas à pas (REC) : Notes écrites au curseur rouge · Ailleurs : Avec SAVE tenu : emplacements ; Arrange : scènes |
| noire 7 (G#4) | Grille 16×8 r4 c9 | Jeu : Notes ; sur une piste batterie : les 11 sons · Steps (SEQ) : Batterie : choisit le son ; pas tenu + noire : ajoute ou retire le son · Ailleurs : Arrange et Mix : pistes 1 à 8 (allumée si la piste joue) |
| blanche 10 (A4) | Grille 16×8 r5 c9 | Jeu : Notes (la piste choisie) · Steps (SEQ) : Pas 1 à 16 : tapé = pose ou efface, tenu = édite le pas · Pas à pas (REC) : Notes écrites au curseur rouge · Ailleurs : Avec SAVE tenu : emplacements ; Arrange : scènes |
| noire 8 (A#4) | Grille 16×8 r4 c10 | Jeu : Notes ; sur une piste batterie : les 11 sons · Steps (SEQ) : Batterie : choisit le son ; pas tenu + noire : ajoute ou retire le son · Ailleurs : Arrange et Mix : pistes 1 à 8 (allumée si la piste joue) |
| blanche 11 (B4) | Grille 16×8 r5 c10 | Jeu : Notes (la piste choisie) · Steps (SEQ) : Pas 1 à 16 : tapé = pose ou efface, tenu = édite le pas · Pas à pas (REC) : Notes écrites au curseur rouge · Ailleurs : Avec SAVE tenu : emplacements ; Arrange : scènes |
| blanche 12 (C5) | Grille 16×8 r5 c11 | Jeu : Notes (la piste choisie) · Steps (SEQ) : Pas 1 à 16 : tapé = pose ou efface, tenu = édite le pas · Pas à pas (REC) : Notes écrites au curseur rouge · Ailleurs : Avec SAVE tenu : emplacements ; Arrange : scènes |
| noire 9 (C#5) | Grille 16×8 r4 c12 | Jeu : Notes ; sur une piste batterie : les 11 sons · Steps (SEQ) : Batterie : choisit le son ; pas tenu + noire : ajoute ou retire le son · Ailleurs : Arrange et Mix : pistes 1 à 8 (allumée si la piste joue) |
| blanche 13 (D5) | Grille 16×8 r5 c12 | Jeu : Notes (la piste choisie) · Steps (SEQ) : Pas 1 à 16 : tapé = pose ou efface, tenu = édite le pas · Pas à pas (REC) : Notes écrites au curseur rouge · Ailleurs : Avec SAVE tenu : emplacements ; Arrange : scènes |
| noire 10 (D#5) | Grille 16×8 r4 c13 | Jeu : Notes ; sur une piste batterie : les 11 sons · Steps (SEQ) : Batterie : choisit le son ; pas tenu + noire : ajoute ou retire le son · Ailleurs : Arrange et Mix : pistes 1 à 8 (allumée si la piste joue) |
| blanche 14 (E5) | Grille 16×8 r5 c13 | Jeu : Notes (la piste choisie) · Steps (SEQ) : Pas 1 à 16 : tapé = pose ou efface, tenu = édite le pas · Pas à pas (REC) : Notes écrites au curseur rouge · Ailleurs : Avec SAVE tenu : emplacements ; Arrange : scènes |
| blanche 15 (F5) | Grille 16×8 r5 c14 | Jeu : Notes (la piste choisie) · Steps (SEQ) : Pas 1 à 16 : tapé = pose ou efface, tenu = édite le pas · Pas à pas (REC) : Notes écrites au curseur rouge · Ailleurs : Avec SAVE tenu : emplacements ; Arrange : scènes |
| noire 11 (F#5) | Grille 16×8 r4 c15 | Jeu : Notes ; sur une piste batterie : les 11 sons · Steps (SEQ) : Batterie : choisit le son ; pas tenu + noire : ajoute ou retire le son · Ailleurs : Arrange et Mix : pistes 1 à 8 (allumée si la piste joue) |
| blanche 16 (G5) | Grille 16×8 r5 c15 | Jeu : Notes (la piste choisie) · Steps (SEQ) : Pas 1 à 16 : tapé = pose ou efface, tenu = édite le pas · Pas à pas (REC) : Notes écrites au curseur rouge · Ailleurs : Avec SAVE tenu : emplacements ; Arrange : scènes |

## Libres

- Boutons : B6, B11 (B6 = clic de l’encodeur : Shift + B6 tenu 2 s = sortie d’Avrek OS)
- Potards : 7, 8, 9, 10, 11, 12, 13, 14, 15
