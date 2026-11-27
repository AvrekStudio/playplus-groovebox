# LOOM-8 en invité sur le Play+

[LOOM-8](https://github.com/charlesvestal/fm1-loom) (Charles Vestal, sur fm1-base) est un firmware de groovebox
pour le M-VAVE FM-1. C'est la base du grand projet : son fork, AvrekStudio/avrek-loom, le modifie pour le Play+.

## Construire

Depuis le 27/11/2026, le code de LOOM et notre couche Play+ (`plat_playplus.c`) sont dans le fork
**AvrekStudio/avrek-loom** (privé, GPL-3.0), où LOOM se modifie librement (fusion). Ici restent la matrice, `gen_map.py`,
`loom.ld` et `shim/`, partagés avec les autres invités.

```sh
python3 guest/loom/build_loom.py            # -> guest/loom/loom.pgi (non versionné)
```

Le script trouve le fork (`$AVREK_LOOM`, `../avrek-loom`, `~/src/avrek-loom`, sinon il le clone) et lance son
`playplus/build.py`. Copier `loom.pgi` dans le dossier `Guests` de la carte SD (nom `LOOM-8.pgi`).

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

## Sauvegarde

SAVE (pad de fonction, colonne 19, 2e rangée) écrit le projet sur la carte : un fichier par objet
d'fm1-base dans `Guests/LOOM-8/` (`o00.bin`, `o01.bin`...), relus au chargement de LOOM-8. Vérifié sur le
Play+ virtuel : le projet revient après un redémarrage complet.

## Ce qui manque encore

- Second cœur (LOOM-8 calcule tout sur l'unique cœur du Play+, à 600 MHz). Depuis le 27/11 : un rendu de 128 trames
  par appel audio (avant, 256 un appel sur deux : craquements à PIC 140 %) et une horloge à 24 MHz comme celle du
  FM-1 (le garde-fou de charge et l'admission des voix de LOOM comptent en tics de 24 MHz). Voir `plat_playplus.c`.
- MIDI, Bluetooth, USB, banque de samples utilisateur.
- Les bandes de 40 px de chaque côté de l'image : libres (étiquettes des potards, à venir).
