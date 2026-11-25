# Invités PGI (mode invité, chargement depuis la carte SD)

Un **invité** est un programme à nous (ou un portage, comme LOOM plus tard) que le Play+ charge depuis la
carte SD et fait tourner « au-dessus » du firmware Polyend : il reçoit le son, les pads, boutons, potards,
encodeur, et pilote les LED des pads. Format et règles : `cfw/src/pgi.h`.

## Construire et installer

```sh
python3 guest/build.py guest.pgi guest/demo/demo.c
```

Copier les fichiers `.pgi` dans le dossier **`Guests`** de la carte SD (autant qu'on veut, 32 listés au
plus, noms de moins de 28 caractères), puis sur le Play+ : *Menu → Mode invite*.

- **Sélecteur** : la liste des invités à l'écran, un pad bleu par invité (le choisi en blanc).
  Encodeur + clic, ou directement le pad de l'invité : il est chargé et prend la main.
- **Changer d'invité à la volée** : Shift + clic de l'encodeur ramène au sélecteur.
- **Sortie** : Shift + Back, Polyend reprend tout.
- Sans dossier `Guests` (ou vide), le fichier `guest.pgi` à la racine est chargé directement ; sans
  lui non plus, le mode invité affiche sa mire de test. Un fichier abîmé est refusé (« Fichier refuse »).

## Écrire un invité

```c
#include "pgi_guest.h"
static void init(const struct pgi_host *h) { ... }               /* une fois */
static void audio(short *l, short *r, unsigned int n) { ... }     /* SOUS INTERRUPTION, 128 échantillons */
static void event(unsigned int kind, unsigned int a, unsigned int b) { ... }
static void tick(void) { ... }                                    /* boucle principale */
PGI_GUEST("nom", init, audio, event, tick);
```

- Lié à 0x20200000 (OCRAM, 512 Ko pour le code, les données et `.bss`), C normal avec variables globales.
- Pas de libc (seulement `memcpy`, `memset`, `memmove`), pas de libm : approximations à la main.
- `audio` : rapide, sans fichier ni allocation. FPU disponible (simple précision conseillée).
- LED : `h->led(i, r, g, b)` avec i = rangée × 20 + colonne.

## Exemples

- `demo/` : instrument à pads (pentatonique, 8 voix, LED colorées par rangée qui suivent l'enveloppe).
- `bip/` : exemple minimal (un bip carré par pad, LED blanche).
