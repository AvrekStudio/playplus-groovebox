# LOOM-8 en invité sur le Play+

[LOOM-8](https://github.com/charlesvestal/fm1-loom) (Charles Vestal, sur fm1-base) est un firmware de groovebox
pour le M-VAVE FM-1. Ici, il tourne **tel quel** sur le Play+, en mode invité : seule la plateforme d'fm1-base
(`plat.h`) est réécrite pour le Play+ (`plat_playplus.c`, à nous).

## Construire

```sh
python3 guest/loom/build_loom.py            # -> guest/loom/loom.pgi (non versionné)
```

Le script clone LOOM-8 à un commit fixé dans `~/src/fm1-loom` (`LOOM=...` pour un autre dossier), génère ses
en-têtes avec son propre outil (il construit au passage son simulateur PC, avec gcc), compile pour le Cortex-M7
et lie à 0x81E00000. Copier `loom.pgi` dans le dossier `Guests` de la carte SD.

**Licences :** LOOM-8 et fm1-base sont MPL-2.0 fichier par fichier, GPL-3.0 pour le tout. Le `.pgi` est pour
un usage personnel : ne pas le distribuer.

## Mémoire

654 Ko de fichier, 1,13 Mo une fois chargé (dont 300 Ko de tampons d'effets et de motifs) : trop pour l'OCRAM.
LOOM-8 va dans le **haut de la mémoire des samples** de Polyend (2 Mo à 0x81E00000). Le Play+ refuse de le
charger si les samples du projet ouvert y arrivent (plus d'environ 25 Mo de samples).

## Commandes (provisoire, à régler machine en main)

| FM-1 | Play+ |
|---|---|
| clavier (27 touches, F3..G5) | rangées du bas : blanches sur la rangée 8, noires sur la rangée 7 (colonnes 1 à 16) |
| les 14 boutons | pads de fonction, colonnes 17 à 20 des rangées 1 à 4 : FX ENV LFO EDIT / GLO HOME SAVE ARP / SEQ PLAY REC SEL / OCT− OCT+ |
| PLAY / SEL / HOME / REC | aussi Play, Shift, Back, bouton logiciel 3 |
| SELECT | encodeur de l'écran |
| K1..K4, ALGO, PRESET, MASTER | potards 1 à 7 |

Shift + Back et Shift + clic de l'encodeur restent au mode invité (sortie, choix d'un autre invité).

## Ce qui manque encore

- Sauvegarde des projets (ils vivent en RAM, perdus à la sortie) : à brancher sur la carte SD.
- Second cœur (LOOM-8 calcule tout sur l'unique cœur du Play+, à 600 MHz) : mesure de charge à faire sur la machine.
- MIDI, Bluetooth, USB, banque de samples utilisateur.
- Les bandes de 40 px de chaque côté de l'image : libres (étiquettes des potards, à venir).
