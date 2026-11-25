# Fenêtre du Play+ virtuel (`viewer.py`)

L'écran du Play+ en direct, ses 160 pads avec leurs vraies LED, les potards et les boutons, à la souris et
au clavier. Pour se promener dans les menus, l'Explorateur, le mode invité et LOOM-8 sans la machine.
**Lent (environ 25 fois moins vite que la machine) et sans son** : pour voir et naviguer, pas pour jouer.

## Installation (Windows 10 / 11)

Le plus simple : **WSL** (Ubuntu dans Windows ; sous Windows 11 les fenêtres Linux s'affichent seules).

```sh
wsl --install                       # une fois, dans PowerShell (administrateur), puis redémarrer
# dans Ubuntu :
sudo apt install git python3-venv python3-tk mtools dosfstools clang lld llvm
git clone https://github.com/AvrekStudio/playplus-groovebox && cd playplus-groovebox
cp /mnt/c/Users/<toi>/Downloads/PlayPlus_v1.3.0.ppf .      # le firmware officiel, à la racine
scripts/setup.sh
SANS_LOOM=1 scripts/package.sh       # construit le firmware et les invités dans dist/
```

## Lancer

```sh
.venv/bin/python tools/viewer/viewer.py --guests dist/Guests
```

Premier lancement d'une image : environ 10 min (la machine démarre), puis quelques secondes.

| Souris | |
|---|---|
| clic sur un pad | appui (tenu tant que le bouton de la souris l'est) |
| molette sur un potard / clic | le tourner / le toucher |
| molette sur l'écran / clic | l'encodeur / son clic (Menu) |
| clic sur un bouton du bandeau | appui (G1, G2 : les boutons à gauche de l'écran) |

| Clavier | |
|---|---|
| Espace | Play |
| Échap | Back |
| Entrée | Menu (clic de l'encodeur) |
| Maj | Shift (Maj + Échap : sortie du mode invité) |
| ← → | encodeur |
| F12 | capture de l'écran (PNG) |

`--vitesse 50` : plus de temps machine par image (plus fluide pour la musique, moins réactif).
