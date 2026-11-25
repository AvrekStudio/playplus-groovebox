# Invités PGI (mode invité, chargement depuis la carte SD)

Un **invité** est un programme à nous (ou un portage, comme LOOM plus tard) que le Play+ charge depuis la
carte SD et fait tourner « au-dessus » du firmware Polyend : il reçoit le son, les pads, boutons, potards,
encodeur, et pilote les LED des pads. Format et règles : `cfw/src/pgi.h`.

## Construire et installer

```sh
python3 guest/build.py guest.pgi guest/demo/demo.c
```

Copier `guest.pgi` à la **racine** de la carte SD, puis sur le Play+ : *Menu → Mode invite*.
Sortie : **Shift + Back**. Sans fichier (ou fichier refusé), le mode invité affiche sa mire de test.

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
