# LOOM-8 : son ADN, pour le fusionner avec le Play+

*Dossier de référence du grand projet, commencé le 27/11/2026 (compagnon de `PLATEFORME.md`). Source étudiée :
charlesvestal/fm1-loom au commit `22d9613` (fm1-base `7337375`), clonée dans `~/src/fm1-loom` ; les chiffres de
lignes et de mémoire viennent de ce commit et de notre construction (`guest/loom`). Rien n'est recopié ici : des
descriptions et des noms de fichiers. Les décisions du propriétaire : `PLATEFORME.md` §0 (base LOOM, aucune
rétrocompatibilité, pas de MIDI au début).*

## 1. Ce qu'est LOOM-8

Une groovebox complète pour le M-VAVE FM-1 (écran 240 × 240, 14 boutons, 27 touches, 7 encodeurs, 2 cœurs à
360 MHz) : 8 pistes instrument, 11 moteurs, séquenceur 64 pas avec verrous et « tricks » (à la OP-XY), joueurs
(arp, strum, hold, random), un « conducteur » qui mène une grille d'accords, des « throws » (effets momentanés),
« weave » (croiser deux patterns), 2 pistes d'effets en envoi, master (EQ, compresseur, limiteur, pump, couleur),
32 scènes et un morceau. ~25 000 lignes de C pour l'application (`app/`), sur la plateforme **fm1-base** (`base/`).
Son auteur dit s'inspirer de la façon de travailler de l'OP-XY. Manuel : `docs/MANUAL.md` du dépôt (1 741 lignes).

**Licence** : MPL-2.0 fichier par fichier, mais le moteur acid (`dsp/bass303.c`, X0X / Open303 / jc303 / dm-Rat) et
les modèles 909 (`dsp/d909.c`, 9W9 / ER-99) sont **GPL-3.0-only** : le firmware complet est sous GPL-3.0. Braids
(MIT), les modèles « x » (MACHINEDRUM, MIT), Sophie (MIT), les réverbes SPU (psx-spx) : crédités dans
`LICENSING.md`. **Conséquence pour le grand projet** : un fork de LOOM se publie sous GPL-3.0, avec ses sources ;
il ira dans **son propre dépôt** (pas dans playplus-groovebox, qui ne contient que notre code et ne publie aucun
binaire). À décider avec le propriétaire au démarrage.

## 2. Architecture

| Partie | Fichiers | Lignes | Rôle |
|---|---|---|---|
| Contrat plateforme | `base/include/plat.h` | 800 | tout ce que l'appli demande au matériel (`plat_*`), plus `lcd_*` / `gfx` |
| Point d'entrée | `app.c`, `app.h` | 460 | `app_init`, `app_render` (audio), `app_frame` (interface), le projet en mémoire |
| Moteur audio | `engine.c`, `engine.h` | 1 940 | sous interruption : séquenceur, voix, envois, FX, master ; file de commandes depuis la boucle ; garde-fou de charge ; second cœur |
| Séquenceur | `seq/seq.c`, `seq.h`, `pattern.c`, `pattern.h` | 4 950 | au sample près, tricks, verrous, joueurs, conducteur, throws, weave, horloge |
| Synthés | `dsp/synth.c`, `synth.h`, `braids.h` | 5 340 | 16 voix, 8 moteurs de voix, enveloppes, filtres, caractère, LFO |
| Batterie | `dsp/drumline.c`, `d909.c`, `digisophie.c` | 3 170 | le « kit » : 11 touches, ~30 modèles |
| Acid | `dsp/bass303.c` | 1 050 | TB-303 (GPL) |
| Samples | `dsp/sample.c`, `samples.c` | 820 | banque ADPCM, moteurs sample et slice |
| Effets | `dsp/fx.c`, `fx.h`, `psxrev.h` | 1 780 | FX I / FX II (chorus, delay, lofi, phaser, réverbes), master |
| Interface | `ui.c`, `pages.c`, `gfx_loom.c` | 11 600 | toutes les pages, les gestes, le dessin (polices compactes, anti-aliasing) |
| Projets | `project.c`, `lz.c` | 1 320 | sauvegarde en flash par objets, patterns compressés LZ |
| Divers | `midicc.c`, `bt.c`, `web.c`, `render.c`, `res.c` | 780 | MIDI CC, Bluetooth, build navigateur, export WAV |
| Simulateur PC | `host.c` + `base/host/plat_host.c` | 4 970 | **toute l'appli sur ordinateur**, avec commandes de test et contrôles (précieux pour nous) |

**Deux mondes séparés** (règle à garder) :
- **Interruption audio** : `engine_render(lr, n, gain)` → séquenceur (`seq_until_event` / `seq_advance` : le bloc
  est découpé aux instants exacts des événements), voix, kits, envois, FX, master, conversion 24 bits.
- **Boucle** : `app_frame` → `ui_frame` (entrées, pages, dessin). Elle ne touche jamais l'état audio directement :
  **file de commandes** de mots de 32 bits (un producteur, un consommateur : `engine_set`, `engine_key`,
  `engine_scene`…). Les données des patterns sont lues par l'interruption et écrites par l'interface **en place** :
  uniquement des octets ou des mots de 32 bits alignés (pas de lecture à moitié écrite).
- **Anticipation du séquenceur** : le séquenceur peut tourner un bloc à l'avance (« look-ahead », `la_*` dans
  engine.c) pour lisser le coût ; sur le FM-1 une partie (le retour des effets) tourne sur le second cœur.

## 3. Le modèle de données

- **Pistes** : 8 instruments + 5 pistes auxiliaires = 13 (`NALLTRK`) : **COND** (le conducteur, `AUX_BRAIN`),
  **THROW** (les effets momentanés, `AUX_PUNCH`), **FX I**, **FX II** (les envois), **WEAVE**. 10 sont séquencées.
- **Un son = un tableau d'octets `par[NTP]`** (~280 valeurs) : moteur + 4 boutons, page Play (poly / mono / legato,
  glide, bend, volume), deux ADSR, filtre (5 valeurs), envois, LFO (5 valeurs + forme), caractère (4), page 2 du
  moteur (4), octave, joueur (5 + 4), mix, suivi du conducteur, MIDI, puis les **11 touches de kit × 20 valeurs**.
  **Le même index** sert aux verrous de pas, aux destinations du LFO et aux CC MIDI : une idée simple et puissante,
  à garder.
- **Patterns** : 8 par piste ; `tpat_t` = 64 pas + longueur, échelle de temps (½ à 16 doubles-croches par pas),
  groove, quantification, longueur de note, lissage des verrous, boucle (début / fin) : 776 octets.
- **Pas** (`step_t`, 12 octets) : **jusqu'à 4 notes** (un kit : ses 11 sons en bits), vélocité, longueur (¼ de pas à
  64 pas), décalage (±½ pas), **deux tricks** avec leur valeur, une condition (probabilité ou cycle A:B).
- **Verrous** : 96 « lanes » pour tout le projet ; une lane = (piste, pattern, paramètre) et une valeur par pas.
  Sert aussi aux notes de kit (accord, vélocité par son, réglages par son) et au réservoir du joueur random.
- **Scènes** : 32 ; chacune = le pattern de chaque piste, les mutes, l'état du conducteur, le weave, les mutes par
  son de kit. **Morceau** : 64 cases (scène × 1 à 64 mesures), en boucle ou non.
- Taille totale des patterns : `patterns_t` ≈ **71 Ko** (dans notre construction : symbole `pats`, 0x11794 octets).

## 4. Le séquenceur

- **Au sample près**, tempo 40..300 BPM au dixième, 24 tics par double-croche (décalages, longueurs).
- **Échelle de temps par piste** et longueur par piste : les pistes se décalent (polyrythmies) ; la scène suivante
  part à la barre de mesure ou à la fin du pattern de la piste 1.
- **11 grooves** (swing, lilt, weave, stretch, drunk, jitter, accent, lazy, push pull, sway, swell), un dosage par piste.
- **Tricks** (les 14 de l'OP-XY + flam) : pulse, hold, multiply (ratchets), velocity, ramp up / down, random (notes
  le long de la gamme du conducteur), portamento, bend (10 formes), tonality (ignorer la progression, transposer,
  octave, quinte, tierce…), jump (sauts dans le pattern), skip lock / skip comp / skip (cycles), flam.
- **Conditions** : probabilité 1..99 % ou cycles A:B (1:2 … 8:8).
- **Joueurs** (par piste) : **arp** (1/4 à 1/64, triolets, pointés ; up, down, up-down ×2, as played, random ;
  étendue, longueur, style, glide, stéréo), **strum** (un accord diatonique à partir d'une touche ; jusqu'à 8 notes
  tenues deviennent l'accord, déplacé le long de la gamme ; roll), **hold**, **random** (notes tirées d'un réservoir
  de hauteurs par pattern, probabilités de note / hauteur / rythme / retard / vélocité / longueur, cycles, octaves).
  **Les joueurs jouent aussi les pas** (un pas d'accord est arpégé ou égrené).
- **Conducteur (COND)** : une tonalité, une gamme (10 : majeur, mineur, dorien, mixolydien, lydien, phrygien,
  mineur harmonique, pentatoniques, chromatique), un degré d'accord ; ses pas forment une progression. Chaque piste
  le suit : non, diatonique, transposition, harmonie (note → note la plus proche de l'accord), degré d'accord.
  **C'est le cœur du « vrai chord mode » voulu par le propriétaire.**
- **Throws** : 12 effets momentanés × 2 groupes (percussions / mélodie) : mute, stutter, repeat, echo, build, floor,
  jumble (remplissages), warble, pan, octave, chop, ring ; tenus au clavier ou écrits dans la piste THROW.
- **Weave** : deux patterns croisés (par mesures, par pas, rythme de l'un + notes de l'autre, l'inverse) ; « garder
  comme pattern ». La piste WEAVE décide quelles pistes tissent et quand.
- Horloge : interne, ou MIDI (sortie, entrée 24 ppqn).

## 5. Les moteurs de son

**16 voix de synthé** pour toutes les pistes (6 au plus par piste) ; chaque voix : 2 ADSR (ampli, filtre), filtre
(lp24, ladder, lp12, hp12, bp12 ; « dynamic » pour les kits), **étage de caractère** (drive, fold, crush, ring, tilt,
tape, sampler), LFO (wave, wobble, drift, pump ; synchro ou libre ; 4 destinations par module), envois FX I / II.
Un **LFO partagé** (porté par COND) module les pistes choisies. Calcul en flottant, sans libm (`fastmath.h`),
par blocs de contrôle de 64 échantillons (`SY_CHUNK`).

| Moteur | Ce que c'est |
|---|---|
| **knit** | soustractif, 2 oscillateurs (formes, désaccord, sous-osc., bruit, unison) |
| **rayon** | FM (chute, plancher, keyscale en page 2) |
| **jacquard** | tables d'onde (`WT`, 36 Ko de tables générées) |
| **bellows** | orgue (le plus cher : « un quart de plus que les autres ») |
| **cane** | Braids physiques : pluck, bow, reed, flute, bell, drum (6 voix au plus, lignes de 1 536 échantillons) |
| **voile** | Braids voix : vosim, vowel, choir, whisper (4 voix au plus) |
| **sample** | un sample de la banque, joué à la hauteur de la note : départ, longueur, mode (one-shot, boucle, inverse, aller-retour) |
| **slice** | une boucle découpée en 4 / 8 / 16 / 32 tranches, une note = une tranche ; calée sur le tempo si le sample donne ses mesures |
| **kit** (drumline) | 11 touches × un modèle chacune parmi ~30 : kick, snare, hat, clap, shaker, tamb, reso, conga, tom, FM, sample ; 8 modèles « x » (MACHINEDRUM) ; clang et sophie ; **909** (kick, snare, tom, rim, clap ; hats, ride, crash échantillonnés) ; par touche : 4 (ou 8) boutons, aléatoire, pan, volume, mode (one-shot, gated, loop, choke 1..3), enveloppe, filtre, drive, envois |
| **acid** | TB-303 complet (Open303 + Devilfish + RAT), 2 au plus, mono, accent et slide |
| **MIDI** | joue sur un canal MIDI (8 boutons = 8 CC) — **écarté** (pas de MIDI au début) |

**Effets** : FX I et FX II sont des pistes ; chacune un effet : chorus, delay (10 divisions, ping-pong réglable),
lofi, phaser, ou **réverbe en 11 modes** (plate, les réverbes SPU de la PlayStation : room, studio S/M/L, hall, half
echo, space echo, chaos echo, one echo ; lush = Dattorro), avec filtres passe-haut / passe-bas. **Master** : EQ 3
bandes, couleur (tape, sampler), niveaux percussions / mélodie, compresseur, **pump** (ducking par le kick, une
piste ou le métronome), niveau.

## 6. Les samples sur le FM-1 (la grande limite)

Banque d'usine compilée dans le firmware (IMA ADPCM mono 22 050 Hz : 909, orgue, piano, cordes, flûte…) + banque
utilisateur de **216 Ko** en flash (ADPCM mono, 4 à 48 kHz), envoyée par USB. Les voix lisent l'ADPCM en place avec
un petit cache et des points de reprise. **Sur le Play+** : 27 Mo de mémoire, la carte SD, du 16 bits stéréo
44,1 kHz : c'est là que le projet change d'échelle (deux moteurs : en mémoire / en stream, éditeur d'onde ;
`PLATEFORME.md` §0).

## 7. L'interface

- Écran 240 × 240 (ST7789) dessiné **par le processeur** dans des bandes (`cv_px`, la plus grande 240 × 120) puis
  envoyées (`lcd_blit`). Polices maison compactes (`gen_font.py`), anti-aliasing, images de moteurs animées, mire
  pour un lecteur d'écran (accessibilité) et un « miroir » de l'écran vers un navigateur.
- **31 pages** (`pages.c` enum `P_*`) : Arrange, Song, Steps, Bar, Engine, Play, Play 2, Sub, Osc 2, Envelope,
  Filter, Filter env, Kit filter, Character, Sends, FX I, FX II, Throws, LFO, LFO 2, Players 1-3, Weave, Mix,
  Master 1-3, LFO partagé, Setup, Status. Navigation par boutons de fonction (FX, SEL, ENV,
  LFO, EDIT, GLO, HOME, SAVE, ARP, SEQ, PLAY, REC, OCT±), touches tenues + bouton (verrous : pas tenu + EDIT / ENV /
  FX / LFO), listes SEL, annuler / rétablir, sauvegarde automatique (3 s, au calme).
- Accélération des encodeurs, messages, aide contextuelle par bouton tenu.

## 8. Stockage

Flash interne par objets : 3 emplacements de projet (réglages bruts + patterns compressés LZ), 16 presets
utilisateur, 8 kits, calibration. Formats versionnés (`PROJ_FORMAT 22`, migrations). **Pour nous : tout cela
disparaît** (aucune rétrocompatibilité) au profit de fichiers sur la carte SD, dans un format conçu pour le Play+.

## 9. Coût processeur et mémoire

- Bloc FM-1 : 256 trames (5,8 ms). **Sur le Play+ : 128** (notre correctif du 27/11 ; le moteur l'accepte).
- **Gestion de charge** : coût mesuré par voix (unités de ~1,1 % d'un bloc ; knit 3 à 4,5, bellows 5…), admission
  d'une nouvelle voix selon la charge prévue (`LOAD_ADMIT`), **garde-fou** (voix plafonnées à 8 au-delà de 92 % trois
  blocs de suite ou 85 % en moyenne), 303 en version légère si besoin. Horloge de mesure à 24 MHz sur le FM-1 (nous
  la simulons : `guest/loom/plat_playplus.c` fm1_ticks).
- **Mesuré sur le Play+** (un seul cœur à 600 MHz, code en SDRAM) : jeu chargé ≈ 70 % de pic après le correctif
  128 trames, plus de craquement. La marge réelle dépendra du banc d'essai (code en SDRAM vs OCRAM).
- **Mémoire** (notre construction, ~1,29 Mo en tout) : image écran 240 × 240 (`pp_fb`, 113 Ko, la nôtre), objets de
  sauvegarde (`st_data`, 92 Ko), **patterns 71 Ko**, réverbe lush (`rv2_line`, 64 Ko), bande d'écran (`cv_px`,
  56 Ko), texte de map.json (48 Ko, la nôtre), lignes de delay (2 × 22 Ko), tables d'onde (36 Ko), voix (12 Ko),
  Braids cane (12 Ko), FX (11 Ko). Très loin des 32 Mo du Play+ : **la mémoire ne limitera pas**, sauf les samples.

## 10. Ce que fait déjà notre portage (guest/loom/plat_playplus.c)

Le contrat `plat.h` réalisé sur Avrek OS : rendu 128 trames et horloge 24 MHz simulée ; image 240 × 240 envoyée
par l'hôte ; **matrice de commandes** (map.json, page de câblage) ; **séquenceur sur la grille** (rangées de pas,
pistes en colonne 0, gamme en rangée 6, couleurs des pistes) ; LED ; sauvegarde des objets sur la carte
(`Guests/LOOM-8/oNN.bin`) ; bandes latérales (charge, rôles des potards) ; crans répétés. LOOM lui-même n'est pas
modifié.

## 11. Pour la fusion : garder, adapter, remplacer, retirer

| | Quoi |
|---|---|
| **Garder** (l'ADN) | les moteurs (knit, rayon, jacquard, bellows, cane, voile, kit, acid, sample, slice), le séquenceur et ses tricks, verrous, conditions, grooves, joueurs, **le conducteur**, throws, weave, scènes et morceau, FX et master, la séparation interruption / boucle et la file de commandes, l'idée « un son = un tableau d'index », le **simulateur PC** (tests) |
| **Adapter** | bloc 128 ; échelle de temps et longueurs par piste vers la grille 16 × 8 ; accords et arpèges multi-pistes vers la grille (vrai chord mode) ; nombre de pistes (peut-être moins, plus riches) ; gestion de charge recalée sur le Play+ (un cœur, mesures du banc) |
| **Remplacer** | l'interface dessinée au processeur → **interface native EVE** 320 × 240 (listes d'affichage, animations façon OP-1) ; la navigation à 14 boutons → grille + 15 potards + toucher ; le stockage flash → fichiers sur la carte, format neuf ; les samples ADPCM 216 Ko → PCM 16 bits en mémoire + stream depuis la carte, éditeur d'onde |
| **Retirer** | Bluetooth, WiFi, audio USB, MIDI (pour l'instant), second cœur, mises à jour et sauvegardes USB de fm1-base, banque utilisateur ADPCM, build navigateur, migrations de formats |

## 12. Questions ouvertes (à trancher avec le propriétaire)

1. Nouveau dépôt pour le fork (GPL-3.0), nom du projet.
2. Combien de pistes (8 comme LOOM, ou moins et plus riches) ; lesquelles des pistes auxiliaires garder.
3. Quels moteurs garder tels quels, lesquels alléger (bellows coûteux, MIDI écarté).
4. Disposition de la grille : séquenceur, clavier, accords, scènes, mutes ; rôle des 15 potards et du toucher.
5. Format des projets sur la carte (lisible, versionné, sans rétrocompatibilité à tenir).
