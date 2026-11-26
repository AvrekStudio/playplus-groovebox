# Journal du projet (fil rouge)

Ce fichier est la mémoire du projet. **Toute nouvelle session le lit en entier avant de commencer**, et
ajoute une entrée à la fin de chaque séance de travail : ce qui a été fait, décidé, découvert, ce qui
reste en suspens. On écrit pour quelqu'un qui n'a aucun souvenir des séances précédentes. Les entrées
les plus récentes sont en bas.

## État actuel (à tenir à jour)

- **Avrek OS 0.4.0 (26/11, `playplus-1.3.0-own`, image `PlayPlus_AvrekOS.ppf`) : menu d'allumage, LOOM-8 tourne seul sur
  le Play+** (`/Guests/LOOM-8.pgi` sur la carte, matrice `docs/correspondance/loom-8.json`). Interrupteur
  tenu 1,5 s = extinction ; Shift + clic encodeur 2 s = redémarrage en Play+ d'origine (jusqu'à
  l'extinction). À valider sur la machine. Ensuite : menu de démarrage, compléments par firmware.
- **Cible minimale (26/11) : `playplus-1.3.0-base`** = officiel 1.3.0 + réglage Settings > Knob Touch
  On/Off + potard 1 libéré et Master Volume sur Shift + encodeur + enregistrement différé des
  réglages globaux (craquements du Master). Rien d'autre (pas d'invités, pas
  de Voice Sleep). Version `1.3.0-AVREK-B0.3.0`, image `PlayPlus_AvrekBase.ppf`. C'est elle que le
  propriétaire utilise pour apprendre la machine (et la rendre jouable, petit pas par petit pas).
  Tests : `TARGET=playplus-1.3.0-base scripts/test.sh` (12/12, 14/14, 9/9).
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

## 26/11/2026 (suite) — Potard 1 libéré, Master Volume sur Shift + encodeur

**Demande.** Le propriétaire trouve l'ergonomie d'origine mauvaise (un seul paramètre « courant »,
façade de potards mal exploitée) et veut la reprendre en main, pas à pas. Premier pas : le potard 1
(index 0, « Master Volume / Limiter » dans l'officiel) ne fait plus rien ; le volume général se
règle avec Shift + encodeur cranté.

**Fait (`cfw/src/master_shift.c`, cible base, version B0.2.0).**
- Entrées [1] bouton, [2] potard, [4] encodeur de la table 0x6007b27c détournées (attendus
  0x600d9d95, 0x600d9d3d, 0x600d9e5d) ; [3] toucher reste dans `knob_touch.c`, qui demande à
  `ms_knob_blocked()`.
- Potard 0 : rotation et toucher avalés.
- Shift tenu (bouton 10) : les crans de l'encodeur sont donnés à l'officiel comme des crans du
  potard 0. C'est donc le code d'origine qui règle le volume, avec sa bulle en dB.
- **Piège trouvé au test** : l'encodeur seul règle le *paramètre courant* (`fw_cur_param` =
  0x2001b05c, ajouté à `symbols.ld`). Après Shift + encodeur, Master Volume devenait courant et
  l'encodeur seul continuait de régler le volume. Corrigé : le paramètre d'avant est remis quand
  Shift est relâché.
- Volume observé au virtuel : mot à 0x2001d28c, 104 au départ (0 dB ?), −1 par cran, 101 = −1.5 dB.
- Perdu : l'ancien rôle de Shift + encodeur (pas plus gros), et le Limiter (secondaire du potard 0).
- Test `vdev_master_shift` 13/13 ; `vdev_knob_touch` toujours 12/12.

## 26/11/2026 (suite) — B0.2.1 : pas fin sur Shift + encodeur ; craquements du Master

- Sur la machine, Shift + encodeur sautait de 6 dB par cran : avec Shift tenu, l'officiel fait avancer
  un potard par gros pas (12 unités). Trouvé l'octet Shift de l'officiel : `fw_shift_held` =
  0x20018c4c (app + 0x34, diff mémoire au virtuel). On l'efface le temps de nos crans : 1 cran =
  1 unité = 0,5 dB, comme l'ancien potard. Test 14/14.
- Signalé : **craquements** quand on monte vite le Master entre 0 et +12 dB, **aussi avec l'officiel**.
  Hypothèse : gain appliqué par paliers sans lissage (zipper) et/ou écrêtage au-dessus de 0 dB ; peu
  probable que ce soit l'écran (l'audio tourne sous interruption, l'écran dans la boucle principale).
  Tests demandés au propriétaire pour trancher (autre potard tourné vite, Master sous 0 dB).

## 26/11/2026 (suite) — Enquête sur les craquements du Master (au virtuel)

Le propriétaire précise : les craquements s'entendent **sans aucun son chargé ni joué**, en tournant
vite le Master entre 0 et +12 dB (officiel compris). Mesures au Play+ virtuel (cible base) :
- Changer le Master n'écrit **rien** vers la puce audio : seule une lecture I2C périodique (bus
  LPI2C1/2, adresse 0x28, registres 3 et 4) existe, aussi au repos. Le volume est donc numérique.
- Chaque cran fait redessiner la bulle : ~5 800 écritures SPI sur LPSPI3 (écran EVE) + ~430 GPIO.
- Interruption audio (IRQ 0) : le Master tourné vite ne la bloque pas plus qu'au repos ou qu'un autre
  potard (coupure la plus longue ~1 bloc de 2,9 ms ; le virtuel ne reproduit pas la vraie
  temporisation).
- Un volume numérique appliqué à du silence numérique ne produit rien. Hypothèse retenue :
  **parasite électrique** du trafic écran (ou du bus) vers la sortie analogique, donc matériel.
  À confirmer sur la machine : un autre potard (Filter, Tempo) tourné vite sans son craque-t-il aussi ?
  Casque et sortie ligne ? Si oui : réduire le trafic SPI (ne redessiner que ce qui change) est la
  piste firmware, et c'est compatible avec des animations EVE légères.

## 26/11/2026 (suite) — B0.3.0 : la vraie cause des craquements du Master

Le propriétaire précise : **seul le Master craque**, Filter et Tempo non. Donc pas l'écran.
- `fw_app_update` (0x600c84f5) calcule à chaque tour une empreinte des 15 mots de réglages globaux
  (app + 0x4664 ..., Master à + 0x4674) et, si elle change, appelle `fw_global_settings_save`
  (0x600c848d, bl en 0x600c8576) qui **réécrit `Projects/globalSettings` sur la carte SD**.
  Mesuré au virtuel : 10 crans de Master = 10 écritures ; Filter, Tempo, repos = 0.
- Le fichier (7 octets, format compact) **ne contient même pas le Master** (identique à 104 et à 94) :
  écriture inutile, à chaque cran.
- Correction (`cfw/src/settings_defer.c`) : le bl en 0x600c8576 → `cfw_gs_save` (note « à
  enregistrer » + heure) ; case 1 de la table de l'application (0x60078228) → `cfw_gs_app_update`, qui
  appelle l'officiel puis enregistre vraiment quand rien n'a bougé depuis 1,5 s. Une écriture par
  geste au lieu d'une par cran. Test `vdev_settings_defer` 9/9.
- À confirmer à l'oreille sur la machine. Si ça craque encore, l'écriture SD n'était pas (seule) en
  cause : chercher ce que l'audio fait du Master (copie à 0x200059f4).

## 26/11/2026 (nuit) — Master Volume = ampli casque analogique du codec ; on laisse

- B0.3.0 testé : **ça craque toujours**. L'écriture SD n'était pas la cause (on garde quand même le
  différé : une écriture par geste au lieu d'une par cran, sans inconvénient).
- Remonté la chaîne au virtuel : `fw_app_update` → objet « paramètre modulable » du Master (copie à
  0x200059f4, setter 0x6010dd80, lecture du combiné 0x6010dea0) → 0x600ab8aa : Master = paramètre 8,
  borné 1..128, converti en atténuation 0..127 puis 0x600aa09c écrit le **registre 0x22** du codec
  (I2C, deux canaux). Carte des registres = SGTL5000 : 0x22 CHIP_ANA_HP_CTRL (0 = +12 dB, 0x18 = 0 dB,
  0,5 dB par pas) ; Master 104 → 0x18 = 0 dB, 128 → +12 dB. Init du codec vers 0x600aa1ec :
  0x0E ADCDAC_CTRL = 0 (pas de rampe de volume DAC), 0x10 DAC_VOL = 0x3C3C (0 dB), 0x2E LINE_OUT_VOL
  = 0x1D1D, 0x24 ANA_CTRL = 0x36 (**EN_ZCD_HP déjà actif**), 0x22 = 0x1818. Mute/démute :
  0x600aa400 / 0x600aa438 (bit 4 de 0x24, image à objet + 8).
- Donc : le Master ne règle que la sortie casque, en analogique ; chaque cran fait sauter le
  décalage continu de la sortie → clic, plus fort au-dessus de 0 dB. Le zero-cross n'y peut rien.
- Correction possible (non faite) : ampli casque fixe + Master sur DAC_VOL avec VOL_RAMP_EN. Limite
  0 dB (ou ampli fixe à +12 dB, plus de souffle). **Décision du propriétaire : on laisse, pas grave.**
- Au passage : le Limiter reste accessible (écran de Shift + encodeur, partie basse, en descendant).

## 26/11/2026 (matin) — Nouvelle direction : prendre la main sur le Play+, base 1:1 FM-1 → Play+

- Personnalisation de l'officiel abandonnée (« le play+ est irrécupérable » côté ergonomie). Nouvelle
  direction validée : garder de Polyend seulement le démarrage et les pilotes bas niveau, sans toucher
  au chargeur de démarrage ; notre boucle prend tout le reste ; menu de démarrage listant les firmwares
  de la carte (avec « Play+ d'origine » et « Mise à jour » en secours) ; les firmwares FM-1
  (famille fm1-base) tournent sur notre `plat.h` du Play+.
- Outil : page « Câblage FM-1 → Play+ » (artifact claude.ai, sauvegardes lisibles par Claude) : façade de
  l'émulateur (fm1panel.js de fm1-base, MPL-2.0) et image du Play+ chargée par le propriétaire ;
  rôles des contrôles tirés du manuel de LOOM-8, combinaisons HOME, actions d'encodeur (cran +/−,
  valeur N), couleurs des pads.
- Base 1:1 de LOOM-8 validée : `docs/correspondance/loom-8.json` et `LOOM-8.md` (59 liens ; noire 1 et
  blanche 2 ajoutées par Claude, à confirmer).
- Le brouillon « navigateur de samples » est rangé dans `git stash` (non testé, abandonné).

## 26/11/2026 (matin, suite) — Étape 2 : Avrek OS 0.1.0, la boucle de test qui prend la main

- Nouvelle cible `playplus-1.3.0-own` → `PlayPlus_AvrekOS.ppf` (version `1.3.0-AVREK-OS0.1.0`), un seul
  module : `cfw/src/own.c`. Crochet « bl » en 0x600ad854 (fonction vide au début de la boucle principale)
  → `cfw_own_main`, qui ne revient pas. Notre table de listener (toucher des potards ignoré), notre rendu
  audio (4 voix triangle, une note par pad), notre écran (crochet en 0x600aab34), nos LED (chemins de
  Polyend bloqués). Plus d'application Polyend, ni USB ni MIDI (choix du propriétaire : d'abord un
  firmware fonctionnel).
- Sortie de secours : Shift + B7 tenus 2 s → on remet la table de listener et le rendu d'origine et on
  refait la boucle de l'officiel (`stock_loop`) : le Play+ d'origine revient, avec son menu de mise à jour.
- Au virtuel : ~104 000 tours de boucle/s (contre ~18 000 avec l'application Polyend). Test `vdev_own`
  22/22. Symboles ajoutés : `fw_hw_update`, `fw_hw`, `fw_bg_services`, `fw_bg_service2(_obj)`,
  `fw_app_holder`, `fw_gpio_write`, `fw_periodic_5s(_obj)`.
- Secours ultime (manuel § 11.4) : bouton reset à l'arrière → le chargeur installe un `.ppf` pris au hasard
  dans `/Firmware` : n'y laisser que des `.ppf` officiels.

## 26/11/2026 (matin, suite) — Avrek OS 0.1.1 : figé sur le logo à cause de la RAM non effacée

- Sur la machine, 0.1.0 reste sur le logo Polyend. Cause probable : `cfw_ram` (0x20002700) n'est pas
  remise à zéro au démarrage ; elle contenait encore l'état de Knob Touch (nombre magique en tête) et nos
  crochets LED, appelés pendant le démarrage, l'ont pris pour un pointeur → faute mémoire. Le virtuel part
  d'une RAM à zéro et ne le voyait pas. **Règle : tout état trouvé dans cfw_ram se valide avant usage.**
- 0.1.1 : `own()` valide adresse, alignement, magique, auto-référence et heure de prise de main ; prise de
  main différée (3 s après le démarrage, automate d'affichage de Polyend au repos, coprocesseur libre) ;
  retour automatique au Play+ d'origine si notre première image ne part pas en 3 s. Test 23/23, dont un
  cas « RAM sale ».

## 26/11/2026 (matin, suite) — Avrek OS 0.1.2 : les pads passent par l'I2C

- 0.1.1 démarre bien sur la machine (écran « boucle de test ») : boutons, potards et encodeur répondent,
  LED à nos couleurs, mais **aucun appui de pad**. Le virtuel injecte les pads directement au listener, il
  ne pouvait pas le voir.
- Cause : les pads sont lus par **deux contrôleurs en I2C** (adresse 0x28, l'un sur LPI2C1, l'autre sur
  LPI2C2), interrogés par `fw_bg_services` (0x600b0f90 → 0x600aef60 → 0x600ad9bb → 0x600af5c0), que
  0.1.0 ne faisait plus (je le croyais USB / MIDI). 0.1.2 l'appelle à chaque tour : au virtuel, ~300
  interrogations par seconde après la prise de main (0 sans lui). Test 23/23.
- Leçon : le virtuel ne simule pas le matériel des pads ; un test « pads » au virtuel ne prouve rien sur
  la machine. Toute pièce de la boucle de l'officiel retirée doit d'abord être tracée (accès matériels).

## 26/11/2026 (après-midi) — Avrek OS 0.2.0 : LOOM-8 à la place de la boucle de test, interrupteur

- 0.1.2 validé sur la machine : « tout est OK, quasi 0 latence même avec un jeu rapide et chargé ».
- **LOOM-8 dans Avrek OS** (`own_guest.c`, état commun dans `own.h`) : à la prise de main, on charge
  `/Guests/LOOM-8.pgi` (sinon `/guest.pgi`) en SDRAM ; s'il manque, écran de test et nouvel essai toutes
  les 2 s. Avec l'invité : pads, boutons, potards, encodeur lui vont (jamais le toucher des potards, jamais
  l'interrupteur), le rendu audio l'appelle (silence tant que son `init` n'est pas fini : drapeau `ready`),
  l'écran montre son image (240 × 240 au centre, envoyée par rangées entières en un seul transfert SPI
  `fw_lpspi_xfer` 0x600b8bc0 au lieu d'un appel par octet : c'était la lenteur de LOOM en mode invité) et
  les bandes latérales.
- **Matrice** : `guest/loom/gen_map.py` génère `map_playplus.h` depuis `docs/correspondance/loom-8.json`
  au build (`build_loom.py`, autre fichier par `LOOM_MAP=`). `plat_playplus.c` lit ces tables : touches,
  boutons, crans +/−, valeurs d'ALGO, combinaisons « C:A+B », Shift du Play+ (B10) en couche, couleurs
  des pads.
- **Interrupteur (bouton 12)** : chez l'officiel (gestion des boutons 0x600c93b4), appui → objet
  d'extinction app + 0x46a0 (0x600ce23c : départ du chrono, après sauvegarde des réglages), relâché →
  0x600ce258 (annule) ; sa mise à jour (0x600ce268) dessine « Shutting down » et, au-delà de **1500 ms**
  (flottant en 0x600ce2f0), appelle la case 13 (+0x34) de la table de `fw_hw` (0x6003042c) :
  **`fw_power_off` 0x600aaa0d** (écran, LED, puis relâche l'alimentation). Avrek OS fait pareil : B12
  tenu 1,5 s → `fw_power_off(fw_hw)`. Vérifié au virtuel (appel intercepté ; 0,8 s → rien).
- **Sortie de secours** déplacée sur **Shift + clic de l'encodeur (B10 + B6) 2 s** : B7 est PLAY dans la
  matrice. Avec un invité (il occupe la mémoire des samples de Polyend), on ne peut pas rendre la main à
  chaud : on écrit `STOCK_MAGIC` dans `cfw_ram` (mots 1 et 2, qui survivent au redémarrage) puis
  SYSRESETREQ ; au démarrage suivant `cfw_own_main` voit le drapeau et laisse l'officiel. Éteindre
  efface le drapeau : retour à Avrek OS.
- Piège du virtuel : un fichier posé sur la carte virtuelle **après** l'instantané de démarrage n'est
  pas vu, le système de fichiers de Polyend garde le secteur de la racine en mémoire. Un échec de
  chargement ouvre donc `/Firmware/-` (autre dossier) pour forcer la relecture au prochain essai
  (utile aussi si la carte change).
- Charge CPU : au virtuel, CYCCNT n'avance quasiment pas dans le rendu audio injecté (0,0 % affiché,
  même en mode exact) ; seule la machine donnera la charge de LOOM.
- Tests : `vdev_own` 23/23 (sortie sur B6, carte nettoyée), **`vdev_own_loom` 10/10** (chargement, image,
  bandes, rendu, pas posés au clavier + PLAY → son, interrupteur, redémarrage vers l'officiel).
  Capture `docs/captures/avrek-os-loom.png`. Version `1.3.0-AVREK-OS0.2.0`.

## 26/11/2026 (fin de matinée) — Avrek OS 0.2.1 : les contrôles avant l'animation, volume

- 0.2.0 sur la machine : « le Play+ prend enfin vie », son magnifique. Mais pendant une animation de
  LOOM (écran d'un moteur) tout devient poussif : pads latents, contrôles ignorés, édition lente. Et le
  volume à fond reste faible. PLAY sur B7 : d'abord signalé muet, puis « ça marche on dirait »
  (sans doute noyé dans la lenteur).
- Cause de la lenteur : `host_screen` envoyait l'image entière (240 rangées, ~115 Ko, ~30 ms de SPI)
  dans le `tick` de l'invité, et LOOM redessine à 60 images/s pendant une animation : la boucle ne
  faisait presque plus que du SPI. Correction : `host_screen` ne fait que noter les rangées changées
  (empreinte FNV par rangée, `row_sum`/`pend` dans `struct own`) ; **`og_flush`** les envoie depuis la
  boucle, en suites contiguës, **2 ms au plus par tour** (DWT), en reprenant où il s'était arrêté. Les
  images en trop sont sautées (on lit les pixels au moment d'envoyer). Les entrées passent donc
  toujours en premier ; l'animation baissera en images/s si besoin.
  Le « gestion par la puce dédiée » : l'écran EST une puce dédiée (EVE), mais LOOM dessine ses pixels
  lui-même (logiciel, 240 × 240) ; il faut les lui transférer. Étape suivante si ça ne suffit pas : le
  transfert par DMA (en tâche de fond, plus rien de bloquant).
- Volume : le Master de Polyend ne tourne plus, le codec restait au réglage du démarrage (≈ 0 dB ou
  moins). Au chargement de l'invité, Avrek OS écrit le volume casque à **+6 dB** (`fw_codec_hp`
  0x600aa09d sur l'objet codec `fw_codec` 0x20003048, relevés au virtuel en tournant le Master de la
  cible base). Le volume se règle ensuite dans LOOM (MASTER = potard 4). Une seule écriture : un clic
  possible au chargement, rien ensuite.
- Diagnostic : le dernier bouton reçu s'affiche en bas à droite (« B7 v » appui, « B7 ^ » relâché).
- Tests `vdev_own` 23/23, `vdev_own_loom` 13/13. Version `1.3.0-AVREK-OS0.2.1`.

## 26/11/2026 (midi) — Avrek OS 0.3.0 : menu d'allumage

- Constat machine : Shift + clic encodeur (0.2.x) redémarre, l'officiel s'affiche un instant puis LOOM
  revient. Le drapeau écrit dans `cfw_ram` (mots 1 et 2) **ne survit pas au reset sur la machine**
  (au virtuel si). Abandonné.
- « Ne se rallume pas » : venait du chargeur USB-C 90 W selon le propriétaire (pas du firmware). On garde
  quand même un garde-fou : si `fw_power_off` revient (alimentation pas coupée), veille muette et le
  prochain appui sur l'interrupteur redémarre.
- **Menu d'allumage** (demandé par le propriétaire) à la prise de main : les `*.pgi` de `/Guests`
  (8 au plus, `og_scan` par la FatFs de l'officiel), « Boucle de test », « Play+ d'origine (mise a
  jour) ». Encodeur + clic (B6), ou pad n de la 1re rangée ; LED : un pad par entrée (bleu firmware, gris
  test, orange Play+, blanc curseur). Le curseur part du dernier choix (`/Guests/boot.txt` : nom du
  fichier, `+` test, `-` Play+ ; écrit seulement s'il change). « Play+ d'origine » rend la main à chaud
  (`give_back`, validé en 0.1.2) : rien n'a pris la mémoire des samples, donc pas besoin de reset.
  Menu vide : nouvelle recherche toutes les 2 s.
- Shift + clic encodeur 2 s avec un firmware lancé : redémarrage → menu.
- Vérificateur : une couleur 0x0e1013 chargée par movw/movt est prise pour une adresse inconnue ;
  texte du curseur en noir (0).
- Tests : `vdev_own` 26/26 (menu vide, pad 0 → boucle de test), `vdev_own_loom` 22/22 (menu, clic,
  boot.txt, 2e démarrage avec curseur sur le dernier choix, encodeur, Shift + B6, interrupteur, veille).
  Capture `docs/captures/avrek-os-menu.png`. Version `1.3.0-AVREK-OS0.3.0`.

## 26/11/2026 (midi, suite) — Nouvelle matrice LOOM-8 (v2)

- Le propriétaire a refait la matrice dans la page de câblage : clavier descendu (blanches en rangée 5,
  noires en rangée 4 de la grille 16×8), HOME sur B1 Fill, GLO sur B2 Patterns, SEL sur B5 Copy, EDIT /
  LFO / ENV / FX / ARP sur la colonne 3 de la grille 4×8 (rangées 2, 3, 4, 6, 7), ALGORITHM 1 à 8 en
  colonne 1, OCT− / OCT+ en r0 c2 / c3 ; PLAY B7, REC B8, SEQ B9, SAVE B4, SELECT B0 / B3 inchangés.
- `docs/correspondance/loom-8.json` remplacé ; la fiche `LOOM-8.md` est maintenant générée par
  `guest/loom/gen_doc.py` (avec la liste des contrôles libres : B6, B11, potards 7 à 15). Seul
  `LOOM-8.pgi` change (la matrice est compilée dedans), pas le `.ppf`.
- `vdev_own_loom` 22/22 (pas posés par les blanches de la rangée 5).

## 26/11/2026 (midi, suite) — Avrek OS 0.4.0 : plus d'écran Polyend au démarrage, matrice v3, potards sans accélération

- **Matrice v3** du propriétaire : ALGORITHM passe sur le **potard 15** (rotation, crans bruts), SELECT :
  B3 = cran +, B0 = cran − (inversés). `loom-8.json` remplacé, fiche régénérée.
- **Accélération des potards** : le pilote de Polyend (0x600af43c, objet 0x20005058) n'accélère pas :
  position sur 4096 par tour, un cran tous les 2^6 = 64 (octet de décalage en +0x13c), soit 64 crans par
  tour, avec un léger filtre (5·d + 2·d1 + d2) / 8. C'est **LOOM** qui accélère (ui.c, `accel` : ×6 si
  deux crans à moins de 15 ms, ×4 < 30 ms, ×2 < 60 ms), pensé pour les encodeurs du FM-1 ; les crans
  serrés du Play+ le déclenchaient tout le temps. `build_loom.py` compile maintenant une copie
  `app_pp.c` / `ui_pp.c` avec `ACCEL_OFF 1` (`LOOM_ACCEL=1` pour la garder). Si 64 crans par tour est
  trop fin ou trop lent, levier suivant : l'octet de décalage du pilote (7 = 32 crans par tour).
- **Écran Polyend au démarrage** : sonde au virtuel depuis le reset : la boucle de l'officiel démarre à
  ~1,96 s (logo encore affiché), son écran principal (Project / Tempo / Live Rec…) est dessiné vers 2,1 s,
  et on prenait la main à 3 s. `TAKE_MS` 3000 → 200 : prise de main au premier tour de boucle où
  l'affichage est au repos (setup déjà fait). Au virtuel : prise de main à moins de 2,1 s (test).
- Tests `vdev_own` 27/27, `vdev_own_loom` 22/22. Version `1.3.0-AVREK-OS0.4.0`.

## 26/11/2026 (après-midi) — LOOM-8 sans aucune retouche de son code ; matrice v4 ; liste SEL à l'encodeur

- Souhait du propriétaire : **ne rien modifier dans LOOM**, pour l'injecter tel quel à chaque mise à
  jour. La copie patchée `app_pp.c` / `ui_pp.c` (0.4.0) est retirée de `build_loom.py` ; LOOM est compilé
  depuis son clone intact. Tout se fait dans notre couche `plat_playplus.c`, qui inclut `app.c` et voit
  donc les variables `static` de LOOM sans les modifier :
  - accélération coupée : `accel_off = 1` à l'init (variable de ui.c que LOOM ne remet à 0 que dans son
    outil de captures d'écran) ;
  - **liste SEL ouverte** (`lst.open`, pages.c) : l'encodeur du Play+ (PRESETS) est envoyé comme
    ALGORITHM, qui change la valeur de la ligne (l'écran dit toujours « ALGO change », c'est voulu) ; le
    curseur reste à SELECT (B0/B3) ; les pads ALGO 1 à 8 sont coupés et éteints (sinon : N crans de
    valeur au lieu de la piste N).
- **Matrice v4** : ALGORITHM de retour sur les pads 1 à 8 (colonne 1 de la grille 4×8), potard 15 libre.
- Le `.ppf` ne change pas (0.4.0) : seul `LOOM-8.pgi`. `vdev_own_loom` 29/29 (accel_off, liste ouverte
  par B5, ROTATE changé à l'encodeur sans bouger le curseur, pads ALGO éteints et sans effet, rallumés à
  la fermeture).
- Cible à terme (propriétaire) : « un choix dans le menu, un fichier firmware + un mappage et c'est
  parti » : chaque firmware FM-1 compilé avec notre couche, et sa matrice JSON lue sur la carte au lieu
  d'être compilée dedans.

## 26/11/2026 (après-midi, suite) — Grille « éclatée » : séquenceur en haut, notes en bas (essai)

- Demande : séparer le jeu de notes et la saisie des trigs, sans modifier LOOM. Un pattern de LOOM =
  **64 pas (4 mesures de 16)** = les 64 pads des rangées 0 à 3 de la grille 16×8.
- `plat_playplus.c` (seulement) : un pad du haut (rangée r, colonne c) met `ui.bar = r` et donne à LOOM la
  blanche c par `key_down` / `key_up` : tout le comportement des pas est celui de LOOM (tapé = trig,
  tenu = édition aux potards, longueur avec un 2e pas de la même rangée, son de batterie choisi). Hors
  Steps / Bar : `page_button(B_SEQ)` d'abord. Le clavier de la matrice, sur Steps / Bar, passe par
  `keyboard_down` (le chemin de LOOM quand SEQ est tenu ou en enregistrement) : toujours des notes ;
  ailleurs (Arrange, SAVE tenu, listes, HOME tenu) il reste le clavier normal. Appuis mis en file par
  `event` et traités au début de `tick` (`sg_run`), hors de LOOM.
- LED des pas (`sg_leds`, seuls les changements) : trig orange (batterie : autre son = orange faible),
  tête de lecture blanche, pas vides bleu faible (mesure affichée) / très faible, au-delà de la longueur
  éteints. Comme dans LOOM, poser un trig plus loin allonge le pattern (pas 36 → longueur 36).
- Limites : pas tenus et longueur au doigt dans une même rangée seulement (LOOM tient les pas par
  mesure) ; Bar : les noires ne règlent plus la gamme (le clavier y joue) ; rangées 6-7 libres (punch
  effects plus tard, souhait du propriétaire).
- `vdev_own_loom` 35/35 (note au clavier sur Steps sans trig, trigs posés et retirés au pad, mesure 3
  affichée, lecture et tête de lecture sur les LED). Capture `docs/captures/avrek-os-loom-grille.png`.

## 26/11/2026 (après-midi, suite) — Rangée 6 = SCALE de la page Bar

- Sur Bar, les noires 1 à 9 de LOOM règlent le **SCALE** de la piste : durée d'un pas en doubles-croches
  (1, 2, 3, 4, 5, 6, 8, 16, 1/2 ; `BAR_SCALE_KEY`), pas une gamme musicale. Le clavier jouant des notes
  sur Bar (grille éclatée), le propriétaire a demandé de mettre ce réglage sur la **rangée 6, colonnes 0 à
  8**, et rien d'autre.
- `plat_playplus.c` : un pad SCALE donne la noire correspondante à LOOM (`key_down` + `key_up`), seulement
  dans les conditions de LOOM (Bar, piste synthé, SEQ pas tenu, pas d'enregistrement, pas de liste ni de
  question ni de pas tenu). LED : éteints hors de ces conditions ; valeur actuelle en blanc, les autres en
  vert faible.
- `vdev_own_loom` 39/39 (éteints hors Bar, allumés sur Bar pour une piste synthé, SCALE 4 puis 1).
  Capture `docs/captures/avrek-os-loom-scale.png`.

## 26/11/2026 (après-midi, suite) — Couleurs des pistes, blanches fixes sur Steps

- Pads ALGO 1 à 8 : la couleur de chaque piste dans le thème de LOOM (`trk_ink`, RGB565 de pages.c,
  suit le thème choisi dans Setup) ; la piste choisie vive, les autres faibles. Les trigs de la grille
  éclatée prennent aussi la couleur de la piste choisie (au lieu de l'orange).
- Rangée 5 (blanches) sur Steps / Bar : LOOM y allume les pas et la tête de lecture (ses blanches y sont
  les pas) ; chez nous ce sont des notes : LED fixes, vives quand une touche est tenue. Ailleurs (Arrange :
  scènes…) les lumières de LOOM restent.
- `vdev_own_loom` 41/41.
