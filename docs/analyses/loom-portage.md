# Étude : porter LOOM-8 (firmware FM-1) sur le Play+ en « mode invité »

*25/11/2026. Source : https://github.com/charlesvestal/fm1-loom (et son sous-module `fm1-base`), lu
au commit du jour. Rien n'est copié dans ce dépôt.*

## Ce qu'est LOOM-8

Une groovebox complète pour le FM-1 (~33 000 lignes de C) : 8 pistes instrument (chacune avec sa
couleur), 11 moteurs (7 synthés dont deux issus de Braids, un 303, sampler, découpeur de breaks, kit de
batterie, sortie MIDI), 97 presets, séquenceur 64 pas avec p-locks, arpèges, « weave », « throws »,
piste de conduite, 2 pistes d'effets en envoi, mixeur, EQ et compresseur master, scènes et morceau.
Son auteur dit lui-même s'inspirer de la façon de travailler de l'OP-XY.

## La bonne nouvelle : une vraie couche d'abstraction matérielle

LOOM est une application posée sur **fm1-base**, une plateforme partagée par les firmwares FM-1 de son
auteur (et d'autres). Tout ce que l'application demande au matériel passe par **`include/plat.h`**
(~800 lignes, fonctions `plat_*`), plus le dessin (`lcd_fill` / `lcd_blit` et `gfx.c`). Il existe
**déjà deux implémentations** :
- `device/plat_fm1.c` : le vrai FM-1 ;
- `host/plat_host.c` : un **simulateur sur ordinateur** (~3 150 lignes), qui fait tourner toute
  l'application (interface, séquenceur, moteurs) sans FM-1.

**Porter LOOM sur le Play+ = écrire une troisième implémentation, `plat_playplus.c`**, sans toucher
à LOOM. Le simulateur d'ordinateur est la meilleure référence : il montre le minimum à fournir. Et
comme les autres firmwares bâtis sur fm1-base parlent la même interface, **la même couche les ferait
tourner aussi**. C'est exactement la plateforme visée.

## Ce que l'application attend, et ce que le Play+ peut fournir

| Service (`plat.h`) | FM-1 | Play+ (mode invité) |
|---|---|---|
| **Audio** : `app_render(lr, n, gain)`, stéréo 24 bits entiers, 44,1 kHz | demi-blocs de 256 trames | notre crochet sur le callback de rendu (déjà en place), blocs de 128 : appeler avec n = 128 (`APP_BLOCK` est configurable) |
| **Écran** : `lcd_fill` / `lcd_blit`, RGB565 | **240 × 240** (ST7789) | puce EVE 320 × 240 : image 240 × 240 en RGB565 dans la mémoire de l'EVE, affichée **à l'échelle 1, pleine hauteur** ; 2 bandes de 40 px libres à gauche et à droite |
| **Boutons** : `plat_buttons()` | 14 (FX, SEL, ENV, LFO, EDIT, GLO, HOME, SAVE, ARP, SEQ, PLAY, REC, OCT−, OCT+) | 6 boutons de fonction + 5 autour de l'écran + pads de fonction |
| **Encodeurs** : `plat_enc(role)` | 7 (SELECT, ALGO, PRESET, K1–K4) | encodeur de l'écran + 15 potards |
| **Clavier** : `plat_keys()` | 27 touches (fa 3 → sol 5) | grille de pads |
| **Volume** : `plat_master()` | potard MASTER | potard Master Volume |
| **Lumières** : `plat_lights` / `plat_leds` | LED des boutons et des touches | LED RGB des pads |
| **Stockage** : `plat_store_*`, `plat_file_*`, `plat_data*` | flash interne | fichiers sur la carte SD |
| **Temps** : `plat_ms`, `plat_ticks24` | | horloge de l'officiel (`fw_millis`), CYCCNT |
| **MIDI** : `plat_midi_*` | USB, BLE, jack | plus tard (USB / jack du Play+) ; au début, rien |
| **Bluetooth, WiFi, audio USB, 2e cœur** | oui | **non** : fonctions vides ; LOOM sait déjà s'en passer (`plat_cpu2_set` renvoie −1 : un seul cœur) |

## Table de correspondance des commandes (première proposition)

**Écran :** l'image du FM-1 au centre (240 × 240). Les deux bandes latérales affichent ce que font les
potards en plus (étiquettes), la piste en cours et le tempo.

**Encodeurs :**

| FM-1 | Play+ | Remarque |
|---|---|---|
| SELECT | encodeur de l'écran (et son clic) | navigation |
| ALGO | potard Note | |
| PRESET | potard Sample | choix du son, comme le choix de sample du Play+ |
| K1, K2, K3, K4 | potards de la rangée du bas : Filter Cutoff, Sample Attack, Repeat Type, Move | ou la rangée du milieu, à essayer |
| MASTER | potard Master Volume | |
| *(en plus)* | 10 potards restants | **accès direct** aux paramètres que LOOM met derrière des pages ou des combinaisons (par exemple tempo, swing, volume de piste, envois d'effets) |

**Boutons :**

| FM-1 | Play+ |
|---|---|
| PLAY | Play (sous l'écran, orange) |
| REC | Live Rec (sous l'écran) |
| SHIFT / SEL | Shift / bouton de gauche du haut |
| HOME | bouton de gauche du bas |
| SAVE | Save |
| FX, ENV, LFO, EDIT, GLO, ARP, SEQ | Fill, Patterns, Copy, View et pads de fonction de la colonne de droite |
| OCT− / OCT+ | 2 pads de fonction |

**Clavier :** les 27 touches deviennent un clavier sur la grille de 8 × 16 pads, par exemple deux
octaves et demie sur les deux rangées du bas en disposition piano, ou un clavier isomorphe sur plus de
rangées. Le reste de la grille sert à montrer les 64 pas du séquenceur de LOOM et les 8 pistes avec
leurs couleurs. **Les pads du Play+ ne sont pas sensibles à la vélocité** : LOOM recevra une vélocité
fixe, ou réglable par un potard.

*À valider avec le propriétaire, machine en main.*

## Puissance (avant mesure)

Le FM-1 tourne à 360 MHz, et LOOM **utilise ses deux cœurs** (le second pour une partie du rendu). Il
surveille sa charge : il se protège au-delà de 80 %. Le Play+ n'a qu'un cœur, mais à 600 MHz, en
Cortex-M7, avec une FPU. En mode invité, le calcul audio de Polyend est remplacé : il reste environ 95 %
du processeur pour LOOM.

Probablement suffisant, mais un projet chargé de LOOM peut occuper beaucoup de ses deux cœurs. **Il faut
mesurer** : compiler le rendu de LOOM pour le Cortex-M7 avec une plateforme minimale et le faire
tourner dans le banc Unicorn, comme pour Braids, sur ses scénarios de test (moteurs, voix, kit qui
joue). LOOM fournit ses propres cas de mesure (`tools/measure_load.sh`) : on peut reprendre les mêmes.

## Mesures (25/11/2026)

Le moteur audio de LOOM (`synth.c` et ses moteurs, effets delay et reverb) compilé pour le
Cortex-M7 du Play+ (clang `-O2`, FPU simple précision) et exécuté dans Unicorn, par blocs de 128 trames
(banc `tools/bench_loom/`). Pourcentage = instructions / 1 741 497 (budget d'un bloc à 600 MHz, en
comptant 1 instruction = 1 cycle).

| Cas | Charge moyenne | Pic |
|---|---|---|
| Silence | 0,5 % | 0,5 % |
| Silence + delay + reverb | 0,9 % | 0,9 % |
| knit (synthé « supersaw ») : 1 / 4 / 6 voix | 1,1 / 3,1 / 4,4 % | 4,6 % |
| rayon : 1 / 4 / 6 voix | 1,8 / 5,6 / 8,2 % | 8,3 % |
| jacquard : 1 / 4 / 6 voix | 1,4 / 4,2 / 6,1 % | 6,3 % |
| bellows : 1 / 4 / 6 voix | 1,5 / 4,5 / 6,5 % | 6,8 % |
| cane (Braids) : 1 / 4 voix | 1,7 / 5,3 % | 5,4 % |
| voile (Braids) : 1 / 4 / 6 voix | 1,8 / 5,7 / 8,3 % | 8,4 % |
| acid (303, monophonique) | 3,8 % | 4,2 % |
| **Chanson chargée** : kit qui joue + acid + knit × 4 + voile × 4 + delay + reverb | **18,3 %** | **18,9 %** |

**Lecture :**
- Une **chanson chargée de LOOM tient dans moins de 20 %** du processeur du Play+ dans ce modèle. Même
  en doublant (le vrai Cortex-M7 peut perdre des cycles sur la mémoire flash externe et les caches), on
  reste vers 40 %, plus environ 5 % de coût de fond Polyend, bien en dessous du seuil de 80 % où LOOM se
  protège lui-même.
- Le pire cas théorique, 16 voix du moteur le plus lourd (voile ou rayon, ~24 000 instructions par voix
  et par bloc) + acid + kit + effets, ferait environ 30 % ici, 60 % en doublant : encore dans les clous.
- **Le Play+, sur un seul cœur, fait tourner LOOM plus confortablement que le FM-1 sur deux.**

**Limites de la mesure :**
- **Non mesuré** : le mixeur et la chaîne master d'`engine.c` (EQ, compresseur, limiteur), le
  séquenceur, l'interface et le dessin (dans la boucle principale, hors audio). Ce sont des coûts
  modestes à côté des voix, à mesurer lors du portage complet.
- Les moteurs **sample** et **slice** sont restés muets : leurs échantillons viennent de la mémoire flash
  du FM-1 (`plat_data`), absente du banc. Leur coût réel est à mesurer avec des échantillons chargés
  depuis la carte SD.
- Instructions, pas cycles : le chiffre réel viendra de *Menu → Diagnostic* sur la machine.

## Licence

LOOM est sous **MPL-2.0** fichier par fichier, mais **GPL-3.0** dans son ensemble à cause du 303 et des
modèles 909. Pour un usage personnel, aucun problème. Pour le dépôt : on ne copie pas LOOM, on le
récupère au moment du build (comme Braids) et notre couche `plat_playplus.c` reste à nous. Si un jour
on distribue des binaires, il faudra respecter la GPL, ou construire LOOM sans le 303 et le 909 pour
rester en MPL-2.0. Et un `.ppf` patché ne se publie jamais, de toute façon.

## Plan

1. **Mesurer la charge de LOOM sur Cortex-M7** (banc Unicorn).
2. **Briques du mode invité côté Play+** : prendre l'écran, intercepter les entrées au niveau du
   listener, piloter les LED des pads, bascule mode Polyend ↔ mode LOOM.
3. **`plat_playplus.c`** : audio, écran, entrées, lumières, temps, puis stockage sur la carte SD.
   Fonctions vides pour BLE, WiFi, audio USB, 2e cœur.
4. **Tester LOOM dans le Play+ virtuel** : captures d'écran, pads, son.
5. Sur la machine : charge réelle, réglage de la table de correspondance.
