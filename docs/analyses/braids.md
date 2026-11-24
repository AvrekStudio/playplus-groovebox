# Étude : Braids sur le Play+ (faisabilité et charge CPU)

*24/11/2026. Mesures réelles du code de Braids compilé pour le Cortex-M7 du Play+, exécuté dans
l'émulateur. Le banc est reproductible : `tools/bench_braids/build_and_run.sh`.*

## En bref

**Braids est très léger pour le Play+.** Ses 48 modèles coûtent entre **23 et 220 instructions par
échantillon**, **108 en médiane**. Même avec des hypothèses prudentes, on tient **une quinzaine de
voix** d'un modèle moyen et **8 voix du modèle le plus lourd**, en plus de la charge d'usine. Le code
est sous licence MIT, compile tel quel pour le Cortex-M7, et produit du son sur les 48 modèles.

Ce qui reste à faire n'est pas une question de puissance mais d'intégration : fréquence
d'échantillonnage, place en flash, branchement sur une piste.

## Mesures

Méthode : Braids (pichenettes/eurorack, commit `f4cfa76`) compilé avec clang `-O2` pour
`cortex-m7` / `fpv5-d16`, sans libc. Chaque modèle est rendu 2048 échantillons après 256 de mise en
route, dans trois réglages (do 3 / do 4 / do 5, paramètres à 0 / moitié / max) ; on garde le **pire**
des trois. Comptage exact des instructions exécutées (hook de blocs Unicorn).

| Modèle | instr/éch. | Modèle | instr/éch. | Modèle | instr/éch. |
|---|---|---|---|---|---|
| CSAW | 93 | DIGITAL_FILTER_LP | 119 | STRUCK_BELL | 123 |
| MORPH | 147 | DIGITAL_FILTER_PK | 120 | STRUCK_DRUM | 119 |
| SAW_SQUARE | 145 | DIGITAL_FILTER_BP | 111 | KICK | 103 |
| SINE_TRIANGLE | 166 | DIGITAL_FILTER_HP | 111 | CYMBAL | 160 |
| BUZZ | 79 | VOSIM | 70 | SNARE | 130 |
| SQUARE_SUB | 152 | VOWEL | 77 | WAVETABLES | 80 |
| SAW_SUB | 149 | VOWEL_FOF | 107 | WAVE_MAP | 142 |
| SQUARE_SYNC | 154 | HARMONICS | 186 | WAVE_LINE | 126 |
| SAW_SYNC | 101 | FM | 54 | WAVE_PARAPHONIC | 132 |
| TRIPLE_SAW | 157 | FEEDBACK_FM | 61 | FILTERED_NOISE | 66 |
| **TRIPLE_SQUARE** | **220** | CHAOTIC_FEEDBACK_FM | 55 | TWIN_PEAKS_NOISE | 49 |
| TRIPLE_TRIANGLE | 108 | PLUCKED | 175 | CLOCKED_NOISE | 23 |
| TRIPLE_SINE | 89 | BOWED | 68 | GRANULAR_CLOUD | 109 |
| TRIPLE_RING_MOD | 70 | BLOWN | 93 | PARTICLE_NOISE | 59 |
| SAW_SWARM | 109 | FLUTED | 125 | DIGITAL_MODULATION | 46 |
| SAW_COMB | 89 | TOY | 77 | QUESTION_MARK | 75 |

## Combien de voix ?

Budget du Play+ : 600 MHz, blocs de 128 échantillons à 44,1 kHz, soit **13 600 cycles par
échantillon**. En retirant le coût fixe d'usine (~24 % sur le simulateur) et en visant 80 % de charge
au maximum, il reste environ **7 600 cycles par échantillon**.

On ne connaît pas le nombre exact de cycles par instruction sur la vraie machine : la mémoire flash
externe et les caches jouent. On encadre entre 1 et 2.

| Hypothèse | Modèle moyen (108) | Modèle le plus lourd (220) |
|---|---|---|
| Optimiste (tout le budget, 1 cycle/instr) | ~70 voix | ~34 voix |
| Réaliste (tout le budget, 2 cycles/instr) | ~35 voix | ~17 voix |
| Prudente (moitié du budget, samples qui jouent, 2 cycles/instr) | ~17 voix | ~8 voix |

Polyend limite ses propres synthés à 8 voix au total ; Braids en tient au moins autant dans le pire des
cas. **À confirmer sur la machine** avec *Menu → Diagnostic*, une fois un premier modèle branché.

## Ce qu'il faut adapter

1. **Fréquence d'échantillonnage.** Braids est conçu pour 96 kHz. Ses tables de hauteur sont calculées
   pour ça, donc à 44,1 kHz tout sonnerait faux. On régénère ces tables pour 44,1 kHz : les scripts
   Python sont fournis dans `braids/resources/`. À 44,1 kHz, les modèles brillants (saw, sync,
   square) aliasent plus qu'à 96 kHz. Option si ça s'entend : rendre à 88,2 kHz et décimer par 2. Le
   coût double, ça reste dans le budget (voir le tableau).
2. **Place en flash.** Le code fait ~46 Ko (dont `digital_oscillator` ~34 Ko) et les tables ~56 Ko,
   soit ~102 Ko : **trop pour notre cave de 16 Ko**. Deux solutions, à combiner :
   - la **région ext** après la fin de l'image (déjà gérée par l'outil, mais plafonnée à 64 Ko :
     relever ce plafond) ;
   - charger les **tables depuis la carte SD en SDRAM** au démarrage.
3. **RAM.** ~17 Ko par voix (états des modèles physiques et des lignes à retard) : 8 voix ≈ 136 Ko, à
   allouer une fois au démarrage en SDRAM ou OCRAM.
4. **C++.** Braids est en C++. Notre build ne compile que du C pour l'instant. Le banc montre que
   clang++ suffit, en mode freestanding, sans exceptions ni RTTI, avec quelques en-têtes de
   substitution (`tools/bench_braids/shim/`). À intégrer dans `cfw/tools/build.py`.
5. **Ce que Braids ne fait pas.** Braids est un oscillateur : hauteur, 2 paramètres (timbre et
   couleur), choix du modèle. Il faut ajouter une **enveloppe et un VCA** (quelques instructions par
   échantillon). Le reste (filtre, overdrive, envois d'effets) peut venir de la chaîne de piste du
   Play+ si on branche Braids à la bonne place.
6. **Branchement.** C'est le vrai chantier, commun à tous nos futurs moteurs : faire jouer une piste
   par notre code. Deux voies, à explorer à l'étape 2 :
   - comme **nouveau modèle de synthé** dans les 3 emplacements (le plus naturel côté utilisateur :
     patches, 6 macros) ;
   - à la place du rendu d'une **voix sample** sur une piste audio.
7. **Licence.** MIT : réutilisation libre, en gardant la notice de copyright d'Emilie Gillet.

## Correspondance avec l'interface du Play+

- **Modèle** : le potard Sample (ou Folder) en mode synthé, ou une macro.
- **Timbre / Couleur** : deux macros (par exemple Filter Cutoff et Resonance, comme les synthés
  d'usine).
- **Hauteur** : la note du pas plus Microtune. Braids attend la hauteur en 1/128 de demi-ton.
- **Enveloppe** : les macros Attack et Decay.
- **Écran** : une scène par famille de modèles (analogique, filtres, formants, FM, modèles physiques,
  percussions, tables d'onde, bruits). C'est un bon terrain pour l'interface façon OP-1.
