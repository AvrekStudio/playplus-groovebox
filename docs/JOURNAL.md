# Journal du projet (fil rouge)

Ce fichier est la mémoire du projet. **Toute nouvelle session le lit en entier avant de commencer**, et
ajoute une entrée à la fin de chaque séance de travail : ce qui a été fait, décidé, découvert, ce qui
reste en suspens. On écrit pour quelqu'un qui n'a aucun souvenir des séances précédentes. Les entrées
les plus récentes sont en bas.

## État actuel (à tenir à jour)

- **Cible minimale (26/11) : `playplus-1.3.0-base`** = officiel 1.3.0 + réglage Settings > Knob Touch
  On/Off, rien d'autre (pas d'invités, pas de Voice Sleep). Version `1.3.0-AVREK-B0.1.0`, image
  `PlayPlus_AvrekBase.ppf`. C'est elle que le propriétaire utilise pour apprendre la machine.
  Tests : `TARGET=playplus-1.3.0-base scripts/test.sh` (12/12).
- Le travail v0.1 (LOOM sur la machine, anti-rebond, Voice Sleep par défaut) est en brouillon non
  testé sur `feat/v0.1`, mis de côté.
- **Version :** `1.3.0-AVREK0.0.1` (socle), dans `cfw/targets/playplus-1.3.0/target.h`.
- **Machine :** Play+ commandé le 22/11/2026, pas encore reçu. Rien n'a encore été flashé sur du vrai
  matériel. Tout est validé sur le Play+ virtuel uniquement.
- **Fonctions :** socle (version, crochet de boucle) ; mesure de la charge audio ; *Menu → Diagnostic*
  (message « Audio xx.x% pic yy.y% »). Fusionné dans `main` (PR n°1, 23/11/2026).
- **Dossier prioritaire du propriétaire :** Note Length sur les pistes audio (impossible dans
  l'officiel). Analyse dans `docs/analyses/note-length.md`.
- **Fonctionnement de la machine :** `docs/PLAYPLUS_FONCTIONNEMENT.md` (synthèse du manuel officiel
  OS 1.3.0 et de la photo du produit) : à lire avant toute fonctionnalité qui touche à l'interface.
- **Mode invité :** M1 (son + entrées) fusionné dans `main` (PR n°2). M2 (écran) fait et testé sur la
  branche `feat/mode-invite-ecran` (`vdev_guest`, 19/19) : *Menu → Mode invite* prend le son, toutes
  les entrées et tout l'écran (mire de test, capture `docs/captures/mode-invite-m2.png`) ; sortie par
  Shift + Back, Polyend redessine son écran. M3 (LED des pads) fait sur la même branche
  (`vdev_guest`, 24/24). Jalons suivants : M3 LED des pads, M4 chargement depuis la carte SD, M5 `plat_playplus.c`
  et LOOM (voir `docs/analyses/loom-portage.md`).
- **M4 (fusionné, PR n°4) :** invité PGI chargé depuis la carte SD (`/guest.pgi`) en OCRAM,
  testé (`vdev_guest_sd`, 12/12). Invité de démonstration dans `guest/demo/`.
- **Sélecteur d'invités** (fusionné) : dossier `Guests` sur la carte, liste à l'écran et sur les pads,
  changement d'invité à la volée (Shift + clic de l'encodeur), `vdev_guest_pick` 13/13.
- **SUR LA MACHINE (25/11 soir) :** flash ok, Diagnostic 48 % au repos, Auto-test 4 × ok, Voice Sleep
  48 → 32 % validé à l'oreille. Voir l'entrée du jour.
- **Voice Sleep (VAP), bandes latérales des invités, fenêtre `tools/viewer/viewer.py`** (branche
  `feat/bandes-viewer`) : charge en silence 23,8 % → 13,3 % sur le virtuel, interrupteur *Menu → Voice Sleep*.
- **Kit d'arrivée (fusionné, PR n°7) :** *Menu → Explorateur* (numéro de chaque commande),
  *Menu → Auto-test* (MPU, mémoire des samples, exécution OCRAM / SDRAM), charge CPU à côté de l'image
  des invités, sauvegarde de LOOM-8 sur la carte, `scripts/package.sh`, **`docs/ARRIVEE_MACHINE.md`**.
- **M5, LOOM-8 (fusionné, PR n°6) :** LOOM-8 tourne en invité sur le Play+ virtuel (`vdev_guest_loom`) :
  interface 240 x 240 au centre de l'écran, séquenceur, son. Captures `docs/captures/loom-*.png`.
- **Prochaine étape (ancienne) :** M5 (`plat_playplus.c` et LOOM : son, image 240 x 240 à l'écran, LED).
- **À vérifier dès réception de la machine :** qu'un invité s'exécute bien depuis l'OCRAM (MPU, caches). Puis suite de l'étape 1 — vraie page de diagnostic (écran EVE à nous, rafraîchie en
  continu), mémoire libre. À la réception de la machine : étape 0 (flash), et relever la vraie charge.
- **Dépôts :** `AvrekStudio/playplus-groovebox` = notre projet. `AvrekStudio/freeplay` = fork de
  FreePlay gardé **pour référence uniquement** (comprendre comment il accède au firmware). On n'en
  reprend pas les fonctionnalités.

## Ce qu'il faut savoir pour reprendre

### Décisions prises

- **22/11/2026 :** FreePlay (sloppyjoo/freeplay) sert de boîte à outils, pas de liste de
  fonctionnalités. Décision du propriétaire : « on codera nos propres fonctionnalités sur le Play+ ».
  On a donc fait un dépôt neuf avec seulement ses outils (build, vérificateur, Play+ virtuel, symboles
  et décalages du firmware 1.3.0, notes), crédités dans `LICENSE-FreePlay`.
- Le propriétaire écrit en français : docs, commentaires de haut niveau et échanges en français. Les
  identifiants dans le code restent en anglais (`cfw_*`, `fw_*`), comme dans les outils repris.
- Workflow GitHub : `main` toujours vert, une branche `feat/...` par fonctionnalité, Pull Request,
  étiquette `vX.Y.Z` à chaque étape validée sur la machine.

### Le matériel et le firmware (vérifié)

- Play+ = NXP **i.MX RT1062**, Cortex-M7 600 MHz, FPU ; OCRAM 1 Mo ; **SDRAM 32 Mo** à `0x80000000`.
- Écran 320×240 via une puce **EVE (FT81x)** en SPI : listes d'affichage, bitmaps, dégradés,
  rotations. Le Play+ virtuel émule cette puce (`cfw/vdev/eve.py`) et fait des captures.
- Audio : blocs de 128 échantillons, chemin audio sous interruption.
- Firmware officiel : `PlayPlus_v1.3.0.ppf` = Intel HEX en clair, sans chiffrement, sha256
  `535a903d…6bc867` (fourni par le propriétaire, jamais commité). Application à partir de `0x60030000`,
  fin d'image `0x60190a64`. Le bootloader (en dessous) n'est pas dans le fichier, on n'y touche jamais.
- Notre code va dans la **cave** `0x6018CA40–0x60190A40` (16 Ko remplis de zéros dans l'officiel).
  Si ça ne suffit plus, il existe un mécanisme de **région ext** après la fin d'image (voir
  `cfw/tools/build.py`, clé `ext` de `target.json`, et le fork freeplay pour un exemple).
- RAM de travail sûre : `cfw_ram` = 256 octets à `0x20002700`. **La carte d'occupation est dans
  `cfw/src/diag.h`** : 0x00–0x0F `loop_state`, 0x10–0x3F `audio_state`, le reste libre.
- **Audio (vérifié sur le virtuel) :** IRQ 0 = fin de DMA audio, routine en ITCM `0x00013a18`, qui
  appelle le callback de rendu via le pointeur RAM `0x2000301c` (`fw_audio_render_cb`), valeur
  `0x600ad379` après démarrage. Un bloc = 128 échantillons à 44,1 kHz = 2,902 ms = 1 741 497 cycles à
  600 MHz. Le DWT CYCCNT (`0xE0001004`) est déjà activé par l'officiel.
- Le Play original existe en deux révisions : K66 (Cortex-M4F) et RT1062 (comme le Play+). Hors sujet
  pour nous, on ne cible que le Play+.

### Les pièges déjà rencontrés

- **clang 18 sous Ubuntu** et le vérificateur : il a pris des constantes pour des adresses. Le
  vérificateur a une liste `KNOWN_CONSTANTS` (`cfw/tools/verify.py`) pour ces cas, avec une explication
  par entrée.
- La vérification croisée avec `ld.lld` de FreePlay plante dès qu'il y a une région ext. Nous n'en avons
  pas pour l'instant ; si on en ajoute une, reprendre le contournement fait dans le fork freeplay
  (branche `setup/projet`, `cfw/tools/build.py`).
- Le Play+ virtuel de FreePlay voulait un export Ghidra privé (`rebuild/export/functions.tsv`) : il
  servait seulement aux noms de fonctions dans les journaux, mais son absence faisait planter tout
  démarrage. Notre `cfw/vdev/boot.py` (`fnnames()`) se rabat sur les `symbols.ld`. **Garder ce
  correctif si on recopie un jour boot.py depuis FreePlay.**
- Premier démarrage virtuel d'une image : environ 10 min (2,3 milliards d'instructions jusqu'à
  23 000 ms). Ensuite, départ depuis l'instantané dans `~/.cache/vplay/<cible>-<sha12>/`.
- Le nom du `.ppf` produit doit être reconnu par la machine (préfixe `PlayPlus`). Notre sortie est
  `PlayPlus_Avrek.ppf`. **Pas encore vérifié sur la vraie machine.**

- **Le Play+ virtuel ne compte pas le temps passé dans les interruptions** : `icount` n'avance qu'entre
  deux tranches d'exécution. Pour mesurer, activer `vp.exact_count(True)` (ajout à nous dans
  `cfw/vdev/vplay.py`) : chaque instruction compte, 1 instruction = 1 cycle. C'est un ordre de grandeur.
- **Pas de division 64 bits** dans notre C (pas de bibliothèque d'exécution). Et le vérificateur refuse
  les constantes « qui ressemblent à des adresses » (< 0x100000) : clang fusionne parfois deux
  divisions par des constantes en une seule (ex. /344/1741 → /598904 = 0x92378, refusé). Solution :
  faire une des divisions ailleurs (ici, la moyenne est publiée par l'ISR).
- Le journal des commandes EVE du virtuel (`vp.eve.cmd_log`) s'arrête à `max_log` entrées, déjà
  atteint après le démarrage : le vider avant d'observer l'écran (voir `vdev_diag.py`).

### Où trouver quoi

| Quoi | Où |
|---|---|
| Notre code | `cfw/src/` : `version.c`, `loop.c`, `audio_load.c`, `menu.c`, `diag.h` (état partagé + carte de cfw_ram), `fmt.h` (texte sans libc) |
| Crochets (adresse, valeur attendue, symbole) | `cfw/targets/playplus-1.3.0/target.json` |
| Adresses des fonctions de l'officiel | `cfw/targets/playplus-1.3.0/symbols.ld` |
| Décalages dans les structures de l'officiel | `cfw/targets/playplus-1.3.0/target.h` |
| Ce qu'on sait du firmware | `docs/FIRMWARE_NOTES.md` (notes de FreePlay, en anglais) |
| Tests sur le Play+ virtuel | `cfw/tests/vdev_*.py`, outils communs dans `cfw/tests/device.py` |
| Exemples de fonctionnalités complètes (menus, écrans EVE, LFO, audio) | fork `AvrekStudio/freeplay`, `cfw/src/` — à lire pour apprendre, pas à copier tel quel |

## Entrées

### 22/11/2026 — Démarrage du projet

- Étude : le Play+ est un bon candidat (RT1062, 32 Mo de SDRAM, écran EVE couleur). Deux projets de
  référence : `envyuu/play-envyu` (mod du Play original K66, hors sujet) et `sloppyjoo/freeplay`
  (Play+ 1.3.0, outils réutilisés).
- Fork de FreePlay, build et Play+ virtuel testés dans ce fork (slicer 38/38, tap 6/6), puis décision
  de partir sur un dépôt neuf (voir Décisions).
- Dépôt `playplus-groovebox` créé avec le socle :
  - `version.c` : crochet sur le pointeur de chaîne de version (`0x600c83f0`, attendu `0x60078208`).
  - `loop.c` : crochet sur le slot 1 de la vtable de l'application (`0x60078228`, attendu
    `0x600c84f5` = `fw_app_update`). On appelle l'original puis notre code, qui tient à jour
    `loop_state` (signature `AVK1`, tours, dernier ms, plus grand écart après 100 tours).
  - `verify.py` : l'étape « tests émulés » de FreePlay (son `emu_slice.py`) est devenue optionnelle via
    la clé `emu_test` de `target.json`.
- Résultats : build en environ 2,5 s, `Result: **PASS**` ; `vdev_base` 6/6 (version, crochet actif,
  la boucle avance, pas de tour de plus de 200 ms, menu qui s'ouvre et se ferme). Capture du menu
  vérifiée : menu d'origine intact.
- En suspens : flash sur la vraie machine (attente de la livraison) ; étape 1 de la feuille de route.

### 23/11/2026 — Étape 1 (début) : charge audio et Menu → Diagnostic

- Demande du propriétaire : « commencer le boulot sur le simu » (la machine n'est pas encore là).
- **Mesure de la charge audio** (`audio_load.c`) : à chaque tour de boucle, on remplace le pointeur
  RAM du callback de rendu (`0x2000301c`) par `cfw_audio_render`, qui chronomètre l'original avec le
  DWT CYCCNT. Pas de crochet dans `target.json` pour ça (pointeur en RAM) : réinstallation permanente.
  Statistiques par fenêtre de 344 blocs (1 s) : moyenne, pic ; pic depuis le démarrage.
- **Menu → Diagnostic** (`menu.c`) : crochet sur le constructeur du menu racine (`0x600c9820`, attendu
  `0x600e9e59`) ; ligne action ajoutée après les 6 lignes d'origine ; affiche un toast
  « Audio xx.x% pic yy.y% ».
- **Play+ virtuel** : mode `exact_count` (voir Pièges). Mesure à vide : ~414 000 instructions par bloc,
  soit ~24 % du budget à 1 instr = 1 cycle. **À comparer avec la vraie machine** dès réception.
- Tests : `vdev_base` 6/6, `vdev_audio` 8/8 (dont : la mesure couvre 90–100 % de la routine audio
  vue par l'émulateur, donc l'original est bien appelé en entier), `vdev_diag` 3/3. Captures vérifiées.
- En suspens : la vraie page de diagnostic (écran EVE à nous), la mémoire libre (trouver le tas de
  l'officiel), la PR `feat/diagnostic` → `main` à ouvrir/fusionner par le propriétaire.

### 23/11/2026 — Analyse : Note Length impossible sur les pistes audio

- Demande du propriétaire : « premier dossier important », comprendre pourquoi on ne peut pas régler
  la Note Length. Analyse complète dans `docs/analyses/note-length.md`.
- Constats : la Note Length n'existe qu'en mode MIDI / synthé (potard 6 en Shift : Velocity / Note
  Length, table d'étiquettes `0x6007abb0`). En mode audio, ce potard est Volume / Panning (table
  `0x6007ac28`). Les pas audio et MIDI ont la même structure (0x40 octets, 21 paramètres) ; vtables des
  pas en RAM (`0x20001ddc` audio, `0x20001d74` MIDI) sur le virtuel. Cause de fond : la voix sample
  est en one shot, sans note off (voir FIRMWARE_NOTES).
- Faisable chez nous : gate par fondu forcé via `fw_reader_gain`, au départ de note via le chemin
  `cfw_tgt_note` (voir le fork freeplay), longueur stockée dans un paramètre libre (16–20, à vérifier)
  ou dans notre propre fichier.
- Outil d'exploration utilisé : image extraite dans `/tmp` (jamais dans le dépôt), capstone pour
  désassembler, sondes Python sur le Play+ virtuel (`vp.call_fn` sur les vtables des pas).

### 23/11/2026 — Analyse : charge audio et intérêt de retirer les moteurs Polyend

- Question du propriétaire : le Play+ suffit-il pour une groovebox 16 pistes façon OP-XY ? Se séparer
  des moteurs Polyend libère-t-il de la puissance ? Détails dans `docs/analyses/charge-audio.md`.
- Mesure sur le virtuel : ~24 % de coût fixe même sans aucun son (≈414 000 instructions par bloc),
  inchangé quand on ajoute des pas. Les moteurs ne coûtent probablement qu'en jouant ; le levier, c'est
  le coût fixe (effets, chaînes de piste, mixage), à identifier.
- Piège : un hook Unicorn ajouté **après** que du code a déjà tourné ne voit pas les blocs déjà
  traduits. Installer les hooks de profilage avant tout `run_ms`.

### 24/11/2026 — Lecture du manuel officiel du Play+

- Le propriétaire a fourni le manuel (« Play+ Manual Rev 2 », OS 1.3.0, PDF) et une photo du produit.
  Synthèse dans `docs/PLAYPLUS_FONCTIONNEMENT.md`. Le PDF et la photo ne sont **pas** dans le dépôt
  (droits de Polyend / Synthdawg) : redemander au propriétaire si besoin.
- Faits importants :
  - 16 pistes (8 audio + 8 MIDI / synthé), bascule Audio/MIDI.
  - Grille 8×20 RGB (16 pas + Mute / Solo / Variation / Select), non sensible à la vélocité.
  - 15 potards tactiles ; secondaire par **double tap** (pas Shift).
  - 3 emplacements de synthé × 6 modèles, **8 voix de synthé au total**.
  - Note Length = secondaire du potard Volume en mode MIDI / synthé.
  - Perform uniquement en mode audio ; audio USB 14 canaux stéréo.
- Corrections : `analyses/note-length.md` (double tap, pas Shift) ; `analyses/charge-audio.md`
  (plafond de 8 voix, avertissement sur l'audio USB).

### 24/11/2026 — Décision : alléger les fonctions d'usine inutilisées

- Le propriétaire veut **écrémer les fonctionnalités qui ne lui servent pas** pour récupérer de la
  puissance ; premier candidat cité : **l'audio USB**. Ajouté comme étape 1 bis de la feuille de
  route : mesurer chaque morceau du coût fixe, puis proposer des coupures réversibles.

### 24/11/2026 — Étude Braids et samples mono

- Le propriétaire n'a **pas besoin de samples stéréo** : ajouté aux candidats de l'étape « alléger »
  (à mesurer : lire en mono économise-t-il vraiment du CPU dans la chaîne de piste ?).
- **Braids mesuré** (`docs/analyses/braids.md`, banc `tools/bench_braids/`) : compilé pour le
  Cortex-M7 avec clang++ freestanding (en-têtes de substitution dans `shim/`), 48 modèles exécutés dans
  Unicorn. **23 à 220 instructions/échantillon, médiane 108.** Taille : ~46 Ko de code + ~56 Ko de
  tables (> cave de 16 Ko : région ext à agrandir et/ou tables en SDRAM). RAM ~17 Ko par voix.
- Points à régler : tables de hauteur pour 44,1 kHz (Braids est conçu pour 96 kHz), C++ dans
  `build.py`, enveloppe et VCA, et surtout le **branchement sur une piste** (étape 2).
- Piège : un en-tête `inttypes.h`, `cstring`, etc. manque en freestanding ; les fichiers de
  `tools/bench_braids/shim/` y suppléent. `std::fill` / `min` / `max` y sont réimplémentés.

### 25/11/2026 — CHIP et le SDK `.psyn` de Polyend

- Le propriétaire demande si le moteur **CHIP** (Tracker CE) peut tourner sur le Play+. Étude :
  `docs/analyses/chip-psyn.md`.
- Le SDK `.psyn` (MIT) vise le Cortex-M7, en 44,1 kHz, blocs de 128, float ; son en-tête nomme le
  Play+ ; CHIP a un `type_id_play_tracker` = 8. Le Play+ 1.3.0 ne charge **pas** de `.psyn` (aucune
  chaîne correspondante dans le binaire).
- Piste retenue : un **mini chargeur `.psyn`** dans notre firmware → CHIP et tous les moteurs CE, chargés
  en SDRAM depuis la carte SD (pas de contrainte de flash). Le code hôte `tracker-ce/synth/` sert de
  carte pour l'étape 2.
- **Hypothèse Voice Sleep** : les voix de synthé inactives seraient calculées en permanence dans le
  Play+ 1.3.0 (comme le Tracker avant le CE) → part du coût fixe d'environ 24 % ? À vérifier.

### 25/11/2026 — Hypothèse Voice Sleep : confirmée

- Expérience (silence, polyphonie changée dans *Menu → Synths*) : 23,7 % par défaut ; 12,0 % sans
  Synth 1 (VAP) ; 35,3 % avec VAP à 6 voix ; **5,5 % avec les 3 synthés à Off**. Détails dans
  `docs/analyses/charge-audio.md`.
- Le Play+ 1.3.0 calcule en permanence toutes les voix de synthé allouées. VAP ≈ 3,9 % par voix.
- Navigation utile dans le virtuel : racine du menu → index 2 = Synths → index 0/1/2 = Synth 1/2/3 →
  ligne 2 = Polyphony ; cliquer (`BTN_ENTER`), tourner l'encodeur, cliquer. Script d'exploration
  non versionné (`/tmp/vs2.py`, `/tmp/vs3.py` de la session) ; à transformer en test si on implémente
  le Voice Sleep.
- Priorité n°1 de l'allègement : un Voice Sleep dans notre firmware.

### 25/11/2026 — Remplacer le firmware par un firmware FM-1 ? Début de l'étude du matériel

- Question du propriétaire : remplacer complètement le firmware par un firmware FM-1 (code source
  supposé disponible), avec une table de correspondance des commandes. Réponse : c'est un **portage**.
  Option A, firmware autonome (couche matérielle Play+ complète à écrire, pilotes du Tracker CE comme
  base) : gros chantier. **Option B, « mode invité »** (recommandée et jugée idéale par le
  propriétaire) : le firmware Polyend reste en dessous comme couche matérielle, notre code prend
  l'audio (callback de rendu), les entrées (listener) et l'écran.
- Demande : étudier d'abord comment le firmware accède au matériel. Premiers résultats dans
  `docs/analyses/materiel-entrees-sorties.md` : potards sur ADC1 (canaux 13 et 5) avec multiplexeur
  (GPIO2 bits 16/17/24/25), pads balayés via GPIO2/GPIO3, encodeur sur ENC1, LED en chaîne via LPSPI4
  + eDMA (trame de 7728 octets, hypothèse WS2812 × 160).

### 25/11/2026 — Vision : le Play+ comme plateforme pour les firmwares FM-1

- Le propriétaire veut faire tourner les **firmwares custom du FM-1** sur le Play+ en mode invité :
  plus aucun design graphique à faire (on reprend leurs écrans, agrandis au plein écran du Play+), et
  beaucoup plus d'encodeurs et de pads que sur le FM-1 (écran minuscule, manque de potards).
- Idée technique retenue : une **couche de compatibilité « FM-1 » sur le Play+**, qui fournit aux
  firmwares FM-1 recompilés pour ARM ce qu'ils attendent (image d'écran, touches, encodeurs, callback
  audio, stockage), branchée sur le mode invité. L'écran FM-1 serait copié dans la mémoire de la puce
  EVE puis affiché agrandi par la puce elle-même (coût CPU quasi nul).
- Préalables : le code source des firmwares visés (et leur licence), et repérer leur base commune
  (SDK / couche matérielle partagée). Le kit `fm1-to-smk37` (portage FM-1 → SMK-37) est un précédent
  utile.

### 25/11/2026 — Vision : plateforme pour les firmwares FM-1

- Le propriétaire voit le Play+ comme **la plateforme idéale pour les firmwares du FM-1** : écran plus
  grand et lisible (le FM-1 a un écran « TROP petit, à la limite de l'illisible »), beaucoup plus
  d'encodeurs et de pads. Plus de design à faire : on agrandit l'écran du FM-1 en plein écran. Ajouté
  à la feuille de route (section « Vision »). Attendre le code source d'un premier firmware FM-1 pour
  inventorier sa couche matérielle.

### 25/11/2026 — LOOM-8 : étude de portage

- Le propriétaire a fourni LOOM-8 (https://github.com/charlesvestal/fm1-loom). Étude dans
  `docs/analyses/loom-portage.md`.
- Clé : LOOM repose sur **fm1-base**, une couche matérielle propre (`include/plat.h`, fonctions
  `plat_*` + `lcd_fill`/`lcd_blit`), avec déjà une implémentation FM-1 et un **simulateur sur
  ordinateur** (`host/plat_host.c`). Porter = écrire `plat_playplus.c`. La même couche ferait tourner
  les autres firmwares fm1-base.
- FM-1 : écran 240 × 240 RGB565 (→ pleine hauteur à l'échelle 1 sur le Play+, 2 bandes de 40 px
  libres), 14 boutons, 7 encodeurs, 27 touches, 360 MHz × 2 cœurs, 44,1 kHz, `app_render` en entiers
  24 bits stéréo. Première table de correspondance proposée.
- Licence : MPL-2.0 par fichier, GPL-3.0 dans l'ensemble (303, 909). Ne pas copier LOOM dans le dépôt.
- Prochaine étape proposée : mesurer la charge de LOOM sur Cortex-M7 dans le banc Unicorn.

### 25/11/2026 — Charge de LOOM-8 mesurée sur Cortex-M7

- Banc `tools/bench_loom/` (LOOM cloné à un commit fixé, DSP compilé avec clang pour Cortex-M7, en-têtes
  `string.h`/`math.h` de substitution, aucune fonction libm nécessaire : LOOM utilise `fastmath.h`).
- Résultats (1 instr = 1 cycle) : voix de 1,1 % (knit) à ~1,4 % par voix (voile/rayon) ; acid 3,8 % ;
  **chanson chargée (kit + acid + 8 voix + delay + reverb) : 18,3 %**. Le Play+ fait tourner LOOM
  confortablement, même en doublant les chiffres.
- Non mesuré : mixeur/master d'`engine.c`, séquenceur, interface. Sample et slice muets (échantillons
  absents du banc).

### 25/11/2026 — Mode invité, jalon M1 : son et entrées

- Branche `feat/mode-invite` (partie de `analyse/note-length`, pour garder les docs d'analyse).
- `cfw/src/guest.c` / `guest.h` : état dans `cfw_ram` 0x40–0x7F (`struct guest_state`, 64 o).
- **Entrées :** 5 crochets sur la vtable du listener (0x6007b27c, slots 0 à 4 : pad, bouton, potard,
  toucher, encodeur ; adresses et valeurs attendues reprises de la cible FreePlay, déjà vérifiées).
  Hors mode invité on appelle l'original ; en mode invité l'événement est compté pour l'invité et
  n'arrive pas chez Polyend. Sortie : Shift (10) tenu + Back (7).
- **Son :** dans `cfw_audio_render`, si le mode invité est actif, `guest_render(L, R)` remplace le rendu
  Polyend (triangle 220 Hz, 1/8 de pleine échelle). Les moteurs Polyend ne sont plus calculés du tout.
- **Menu :** action « Mode invite » (sans accent, police incertaine). Le menu est encore ouvert quand
  l'action s'exécute : `guest_loop()` (appelée par `loop.c`) le referme en rejouant Back à l'original
  tant que l'écran courant est le menu (8 essais au plus).
- **Nouveau symbole :** `fw_input_listener = 0x20018c34` (le vérificateur refuse un littéral RAM inconnu).
- **Piège :** le relâché de la touche qui a validé l'entrée du menu arrivait chez l'invité. On ne garde
  un relâché/tenu de bouton que si l'invité a vu l'appui (masque `held`).
- Tests : `vdev_guest` 14/14, et `vdev_base`, `vdev_audio`, `vdev_diag` toujours verts.

### 25/11/2026 — Mode invité, jalon M2 : l'écran

- PR n°2 (M1 + analyses) fusionnée par le propriétaire. Nouvelle branche `feat/mode-invite-ecran`.
- **Comment l'officiel dessine** (désassembleur + trace des `swap` sur le Play+ virtuel) : la boucle
  matériel 0x600aab28 appelle en 0x600aab34 `BL 0x600ab394` = automate d'affichage (dm = objet
  matériel + 4). État en dm+0x14 : 0 cherche un widget modifié, 1 en redessine un dans son cache RAM_G
  (0x2000 o par emplacement), 2 compose l'image (DLSTART, chaque widget visible ajouté depuis son cache,
  DISPLAY, SWAP). Garde : dm+0x34 == 1 et dm+4 != 0. Au repos : 0 image/s ; ~3 images par action.
- **Crochet `bl`** sur 0x600aab34 → `cfw_display_update` (`cfw/src/guest_screen.c`). En mode invité
  (menu refermé) on n'appelle pas l'original et on envoie notre liste d'affichage complète, ~30 images/s,
  seulement quand le coprocesseur est libre. À la sortie : on met l'automate à l'état 2 quand il est au
  repos, Polyend recompose depuis ses caches (intacts : on n'écrit pas en RAM_G).
- Anneau RAM_CMD de 4 Ko : on n'écrit que s'il est vide, image < 1023 mots ; à chaque retour de
  l'offset à 0 on referme/rouvre la rafale. `fw_eve_text` avance `fw_eve_wp` lui-même : on bourre
  jusqu'à la fin de l'anneau s'il n'y a plus la place pour le texte.
- Mire de test (palette à nous) : grille des 160 pads (allumés quand tenus), barre animée, compteurs.
- `guest_state` passe à 96 o (0x40–0x9F de `cfw_ram`) : frames, last_frame_ms, redraw, pads[5].
- Nouveau symbole : `fw_display_update = 0x600ab395`.
- Tests : `vdev_guest` 19/19 (images ~30/s, fond de la mire, pad tenu allumé puis éteint, Polyend
  redessine à la sortie) ; `vdev_base`, `vdev_audio`, `vdev_diag` verts.

### 25/11/2026 — Mode invité, jalon M3 : les LED des pads

- **Pilote LED** (désassembleur + traces) : objet 0x20004d84 = gestionnaire des pads 0x20004d80 + 4
  (vtable 0x60076eb8). +0 état (0 repos, 1 attente, 2 DMA en cours), +4 trame (7728 o en SDRAM),
  +8 « modifiée » (envoi au plus toutes les 20 ms par 0x600afd74), +0x14 file d'attente utilisée si on
  écrit pendant un envoi. Encodeur 0x600af998(pilote, i, r, g, b) ; i = rangée × 20 + colonne (vérifié
  avec les pads (0,4) -> 4, (3,10) -> 70, (7,19) -> 159). Format d'une LED : voir
  `docs/analyses/materiel-entrees-sorties.md`.
- **Chemins d'écriture de Polyend**, crochetés (`cfw/src/guest_led.c`) : slot 1 du gestionnaire
  (0x600ac580 : écriture immédiate si couleur2 == 0 et mode == 0, sinon mémorisation pour le
  clignotement) et BL 0x600aca2a (clignotements). En mode invité on bloque les écritures, on laisse
  passer les mémorisations.
- Entrée : copie de la trame de Polyend dans un tampon du tas (alloué une fois). Sortie : on la remet
  et on la marque modifiée (après la fin d'un envoi en cours). Mire : pads tenus en orange, tête de
  lecture cyan qui avance d'une colonne toutes les 120 ms.
- **Piège :** `symbols.ld` (repris de FreePlay) avait déjà `fw_led_set = 0x600ac581` (le setter du
  gestionnaire, pas l'encodeur). Mon `fw_led_set = 0x600af999` était écrasé par la 2e définition : nos
  couleurs passaient par la mauvaise fonction et corrompaient la mémoire (plantage à la sortie).
  Renommé `fw_led_encode`. **Toujours chercher un nom dans `symbols.ld` avant d'en ajouter un.**
- `guest_state` : 116 o (0x40–0xB3). Tests : `vdev_guest` 24/24 (LED lues et décodées dans la trame),
  `vdev_base`, `vdev_audio`, `vdev_diag` verts.

### 25/11/2026 — Mode invité, jalon M4 : invité chargé depuis la carte SD

- Choix du propriétaire : charger les invités depuis la carte SD (plutôt que dans la flash), pour en
  changer sans reflasher ; à terme, plusieurs invités sur la carte, choisis à la volée.
- **Où mettre le code :** le tas de l'officiel est en DTCM (allocation vue à 0x2004f6f8), or le
  Cortex-M7 n'exécute pas depuis la DTCM. L'**OCRAM 0x20200000** est entièrement vide après le démarrage
  (Play+ virtuel : aucun bloc non nul ; dans l'officiel, seules les tables de démarrage la citent, avec
  une taille 0). La SDRAM est utilisée de façon éparse par des allocations (trame LED à 0x80002800…),
  sans zone libre sûre. → invités liés à 0x20200000, 512 Ko.
- **MPU** (démarrage de l'officiel 0x600b55a8, décodé statiquement) : régions 0–8 et 10 programmées ;
  SDRAM et OCRAM semblent exécutables (XN = 0). On ne s'y fie pas : le chargeur programme la région 15
  (512 Ko à 0x20200000, normale, cache WB, exécutable), nettoie le cache de données et invalide le cache
  d'instructions. Non émulé par le Play+ virtuel : **à vérifier sur la machine**.
- **Format PGI** (`cfw/src/pgi.h`) : image brute liée à 0x20200000, en-tête en tête (magie « PGI1 », ABI,
  fin d'image, fin de .bss, nom, points d'entrée init / audio / event / tick). L'hôte refuse tout
  fichier incohérent (vérifié par le test avec une magie fausse). Services : millis, led.
- **SDK** (`guest/`) : `build.py` (clang + ld.lld + objcopy), `sdk/pgi.ld`, `sdk/pgi_guest.h`,
  `sdk/mini_libc.c`. Invité `demo` : 2,4 Ko.
- **Intégration** : `guest_load.c` (lecture par `fw_file_*`, vérifications, MPU, caches, init) ;
  audio → `pgi->audio` dans `cfw_audio_render` ; événements → `pgi->event` ; `pgi->tick` dans la boucle ;
  la mire LED intégrée se tait quand un invité est chargé ; l'écran affiche le nom de l'invité.
- Test `vdev_guest_sd` : écrit `/guest.pgi` sur l'image de carte avant de repartir de l'instantané, puis
  le retire. 12/12. Piège : l'image de carte n'existe qu'après le premier démarrage de l'image
  (`snapshot_for` d'abord).

### 25/11/2026 — Plusieurs invités sur la carte, choisis à la volée

- Demande du propriétaire : de nombreux firmwares invités sur la carte, chargés à la volée.
- `cfw/src/guest_pick.c` : à l'entrée du mode invité, liste des `*.pgi` de `/Guests` (FatFs de
  l'officiel, tailles DIR/FILINFO comme FreePlay), au plus 32, sur le tas. Sélecteur : écran (liste
  autour du curseur), LED (un pad bleu par invité, curseur blanc), son coupé. Encodeur + clic (bouton 6)
  ou touches 0/3, ou le pad de l'invité. Depuis un invité : Shift + clic → `guest_unload()` puis
  sélecteur (liste relue). Sans `/Guests` : `/guest.pgi` seul, comme en M4.
- `guest_load(chemin)` prend maintenant le chemin ; `guest_unload()` remet `pgi` à 0 avant tout
  rechargement (l'interruption audio cesse d'appeler l'ancien invité).
- 2e invité d'exemple `guest/bip/` (840 o).
- **Simulateur :** Unicorn gardait ses traductions du premier invité après le chargement du second
  (même adresse) → plantage. Ajout dans `cfw/vdev/vplay.py` : une écriture dans SCB_ICIALLU
  (0xE000EF50) jette les traductions de l'OCRAM, comme le vrai cache d'instructions. Le test vérifie
  donc aussi que le chargeur invalide bien le cache.
- `guest_state` : 132 o (0x40–0xC3). Tous les tests verts (6 scripts, 66 vérifications).
- Fusion : le propriétaire a demandé que Claude ouvre et fusionne les PR lui-même (API REST, `gh api`),
  une fois les tests verts. Non inscrit dans `CLAUDE.md` (la modification par Claude de ses propres
  consignes a été bloquée par le garde-fou de la session) : au propriétaire de l'y ajouter s'il le
  souhaite.

### 25/11/2026 — M5 : LOOM-8 tourne sur le Play+ virtuel

- Demande : « teste loom et envoie moi des captures d'écran en taille réelle Play+ ».
- **Portage sans toucher LOOM-8** : `guest/loom/plat_playplus.c` réalise `plat.h` d'fm1-base et inclut
  l'app (unité unique, comme `host/plat_host.c` du simulateur d'fm1-base) ; DSP et séquenceur à part en
  -O2 (liste `o2` de `fm1.toml`). `build_loom.py` clone LOOM-8 / fm1-base aux commits du banc de mesure,
  génère ses en-têtes avec son outil (`base/tools/build.py app --host`), compile avec clang, lie à 0x81E00000.
  Fonctions manquantes au lien : ~40 `plat_*` (bouchons : BLE, USB, second cœur, MIDI, stockage) et
  quelques `__aeabi_*` / fonctions de chaîne (`rt.c`).
- **Taille** : 571 Ko de code et constantes, 478 Ko de RAM (dont `.pool` 303 Ko : tampons d'effets, motifs)
  → 1,13 Mo : trop pour l'OCRAM.
- **SDRAM** : l'allocateur de Polyend (0x600ab610, objet 0x20004d10, « bump » de 32 Mo depuis 0x80000000,
  jamais libéré) a presque tout donné au démarrage : 4 Ko restent. Trace des allocations au démarrage :
  0x1b25e74 o (27,1 Mo) à 0x804d900c par 0x600c9ecd = **mémoire des samples**, remplie par le bas
  (objet en 0x2001b138 : début, capacité en échantillons, curseur = début sur projet vide). On prend son
  haut : 2 Mo à 0x81E00000 ; refus de charger si le curseur y arrive (`fw_sample_pool`).
- **PGI ABI 2** : champ `base` en fin d'en-tête (OCRAM ou SDRAM), service `screen` (image 240 x 240).
  Chargeur : lit l'en-tête seul d'abord, choisit la zone, région MPU 15 sur la zone (512 Ko ou 2 Mo).
- **Écran** : l'image de l'invité va directement en RAM_G de l'EVE à 0xE2000 (libre : polices et images
  de Polyend en 0..0x58000, caches de widgets 0x7a120..0xde120, vérifié sur le virtuel après quelques
  écrans), par transaction SPI avec les fonctions bas niveau de l'officiel (`fw_eve_cs_low`,
  `fw_eve_wr_addr`, `fw_eve_spi_byte`, `fw_eve_cs_high`, `fw_eve_pause`). Seules les rangées modifiées
  partent. Affichage : bitmap RGB565 (poignée 13) au centre, bandes noires de 40 px.
- **Son** : `app_render` par 256 trames (APP_BLOCK), donné en deux fois 128 ; 24 bits → 16 bits.
- **Piège** : le son mesuré à un seul instant tombait entre deux grosses caisses (sortie = 3 ms) :
  mesurer la crête sur 2 s. Le simulateur PC de LOOM-8 (même scénario) sert de référence.
- Commandes : table provisoire (voir `guest/loom/README.md`).
- Reste : sauvegarde (projets en RAM), mesure de charge sur la machine (un seul cœur), MIDI, bandes latérales.

### 25/11/2026 — Kit d'arrivée de la machine et sauvegarde de LOOM-8

- **Explorateur** (*Menu → Explorateur*) : mode invité sans invité ; journal des 4 derniers événements en
  `cfw_ram` (`ev_log`, type | état << 4 | a << 8 | b << 16) affiché sous la grille (« Pad r3 c10 (#70)
  relache », « Potard 4 -3 »...). Pour remplir la table des commandes sur la machine.
- **Auto-test** (*Menu → Auto-test*, `cfw/src/guest_test.c`) : MPU (région active la plus haute hors 15
  couvrant 0x20200000 et 0x81E00000 : XN ?), mémoire des samples (fin du bloc = 0x81ffee80, curseur
  dedans), exécution de « movs r0, #42 ; bx lr » en OCRAM puis en SDRAM. Marque `test_step` posée avant
  chaque test d'exécution (cfw_ram n'est pas effacée au redémarrage) : un test qui a planté est signalé
  PLANTE au lancement suivant et n'est pas relancé. Sur le virtuel : MPU « ECHEC » (registres MPU non
  émulés), le reste ok.
- **Charge CPU** dans les bandes latérales de l'image d'un invité (moyenne 1 s et pic, `audio_load.c`).
  Sur le virtuel : 0,0 % (compteur de cycles non avancé sans `exact_count`).
- **PGI ABI 3** : `file_read` / `file_write` dans `/Guests/<nom de l'invité>/` (noms filtrés, dossiers
  créés par `fw_f_mkdir`, écriture `FA_WRITE | FA_CREATE_ALWAYS` = 0x0a comme FreePlay). `pgi` passe à 1
  seulement après `init` (l'interruption audio n'appelle pas un invité pas encore démarré) ; `pgi_base`
  est posé avant, pour que `init` puisse lire ses fichiers.
- **LOOM-8** : `plat_store_*` sur ces fichiers (un par objet, `oNN.bin`, copie en RAM lue au démarrage).
  Test : SAVE, sortie, redémarrage complet de la machine, LOOM-8 rechargé : les 4 grosses caisses
  reviennent. `cpu_pct` corrigé (600 MHz, pas 600 kHz).
- **Piège** : `mdir -b` donne des chemins complets (« ::/Guests/LOOM-8/o00.bin ») : le nettoyage du test
  ne supprimait rien, et le lancement suivant partait du projet sauvé (les appuis enlevaient les pas).
  `mdeltree` au début et à la fin du test.
- `scripts/package.sh` → `dist/` (firmware + invités, jamais versionné) ; procédure
  `docs/ARRIVEE_MACHINE.md`.
- `guest_state` : 168 o (0x40–0xE7). Cave : 6,9 Ko / 16 Ko.

### 25/11/2026 — Bandes latérales, fenêtre interactive, Voice Sleep (VAP)

- **Bandes latérales (PGI ABI 4)** : champ `bands` en fin d'en-tête = 16 chaînes (8 à gauche, 8 à
  droite, 5 caractères), relues à chaque image ; pointeurs vérifiés dans la zone de l'invité. LOOM-8 y
  met « Pot1 K1 … Pot7 VOL% » (table provisoire). La charge CPU reste en bas des bandes.
- **Fenêtre** `tools/viewer/viewer.py` (+ README) : écran en direct, pads avec les LED lues dans la
  trame, potards (molette), boutons, clavier ; `--guests dist/Guests` ; `--essai` sans fenêtre (vérifié
  ici : tkinter absent de l'environnement de Claude, la fenêtre elle-même est à essayer chez le
  propriétaire, sous WSL).
- **Piège** : l'essai de la fenêtre pointé sur le dossier des tests a mis des invités sur la carte du
  test → échecs des tests suivants. `device.clean_card()` au début de chaque test d'invité.
- **Voice Sleep** : voir `docs/analyses/charge-audio.md` (section du 25/11). Slot 4 des vtables VAP
  0x6008baac / 0x6008bae4 → `cfw_vap_voice_render` : état +0x114 == 0 → tampon à zéro, voix non
  calculée. Test `vdev_voice_sleep` (9/9) : 23,8 % → 13,3 % en silence, une note (événements 4 / 5 de
  l'officiel envoyés au slot 2) réveille la voix, qui se rendort après le relâchement. Inactif au
  démarrage ; `sleep_state` en cfw_ram 0xE8–0xF7.
- Reste pour Voice Sleep : ACD et WTFM (~6,5 points), validation à l'oreille sur la machine.

### 25/11/2026 (soir) — Première fois sur la vraie machine

- Play+ reçu, en **1.3.0**. Flash de `PlayPlus_Avrek.ppf` (image 83d47db770c5…, celle des 97 tests) par
  *System Settings → Firmware → Update Firmware* : redémarrage normal.
- **Diagnostic, projet vide à l'arrêt : 48 % (pic 52,6 %)**. Le virtuel donnait 23,7 % : sur la machine,
  ~2 fois plus (le virtuel compte 1 instruction = 1 cycle ; la machine attend la flash QSPI, la SDRAM,
  les caches, la FPU). **Facteur ~2 à appliquer aux charges mesurées sur le virtuel** (LOOM-8 : 18 %
  virtuel → ~35-40 % attendus).
- **Auto-test : MPU ok, Samples ok, OCRAM ok, SDRAM ok.** Les invités peuvent tourner en OCRAM et en
  SDRAM ; la mémoire des samples est bien là où le virtuel l'a montrée.
- **Voice Sleep (VAP) : 48 % → 32 % (pic 52,6 → 36,4 %)**, soit 16 points. **Validé à l'oreille** par le
  propriétaire (« voice sleep ou pas tout fonctionne ») → à activer par défaut.
- Remarque : l'Explorateur (et l'auto-test, qui s'y affiche) joue la tonalité de test de la mire
  (triangle 220 Hz) : à rendre silencieux.

## 26/11/2026 — Retour à la base : l'officiel + un réglage « Knob Touch »

**Pourquoi.** Sur la machine, LOOM est trop lent à réagir (les potards arrivent par paquets, Play en
appui court ne s'arrête pas, des sons parasites). Le propriétaire veut d'abord **apprendre le Play+
d'origine**, avec une seule retouche : pouvoir couper la détection du toucher des potards (le simple
contact choisit un paramètre, ce qui le gêne).

**Ce qui a été fait.**
- Nouvelle cible `cfw/targets/playplus-1.3.0-base/` (mêmes symboles que la cible principale), avec
  seulement `version.c` et `knob_touch.c`.
- `knob_touch.c` : deux crochets.
  - Entrée [3] (toucher potard) de la table du récepteur d'entrées 0x6007b27c (case 0x6007b288,
    attendu 0x600d9d6d) → `cfw_in_track` : en Off, l'évènement est ignoré (compteur `dropped`).
  - Pointeur du constructeur de Settings dans la table de littéraux 0x600e9f50 (attendu 0x600eac41) →
    `cfw_settings_build` : construit le menu d'origine puis ajoute la ligne « Knob Touch » On/Off avec
    `fw_menu_option_init`.
  - Réglage enregistré dans `/Projects/avrekKnobTouch` (1 octet, 0 = On, 1 = Off ; absent = On).
- Ce qui change en Off : toucher ne choisit plus rien et le double-toucher (paramètre secondaire) est
  perdu ; **tourner un potard choisit toujours son paramètre** (c'est l'officiel qui le fait).
- `scripts/test.sh` accepte `TARGET=...` ; une cible peut lister ses tests dans `tests.txt`.
- Test `vdev_knob_touch` : 12/12 (ligne dans Settings, On/Off, fichier, persistance au redémarrage).
