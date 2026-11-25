# Étude : le moteur CHIP et les plugins `.psyn` de Polyend sur le Play+

*25/11/2026. Source : dépôt `polyend/tracker-ce` (Tracker Community Edition), dossier
`synth-plugin-sdk/` et `docs/custom_synth_sdk.md`.*

## Ce qu'est le SDK

Polyend a publié, avec le Tracker CE, un format de **moteurs de synthé chargeables depuis la carte SD** :
les fichiers `.psyn`.

- **Format** : binaire relogeable pour **Cortex-M7, Thumb-2, FPU simple précision** (le processeur du
  Play+), avec un en-tête de 128 octets. Le code est chargé en **SDRAM** et exécuté depuis là.
- **Interface en C pur** (`psyn_plugin.h`) : création de voix, événements MIDI, changements de
  paramètres, rendu audio, description des paramètres et des **macros** pour l'interface.
- **Format audio** : **44 100 Hz, blocs de 128 échantillons, float [-1, 1]**. C'est exactement le
  format du Play+ : pas de conversion de fréquence (contrairement à Braids, conçu pour 96 kHz).
- **Jusqu'à 8 voix** par moteur.
- **Licence : MIT** pour le SDK et les moteurs open source, dont CHIP. La restriction de la licence CE
  ne vise que le clonage commercial du firmware complet du Tracker.
- **CHIP** : moteur de référence en C++20 (`engines/chip.cpp`, ~1200 lignes). Plusieurs modèles
  (2A03, SID, bruit LFSR, cloche FM, super square, laser…), arpège et accords, vibrato, enveloppe
  ADSR, filtre, bitcrush, glide.

**Indices d'une architecture commune avec le Play+ :** l'en-tête annonce la compatibilité avec
« Polyend Play+ & Original Polyend Play (IMX) », et chaque plugin a un identifiant de type
`type_id_play_tracker` (CHIP = 8). Les moteurs du Play+ (ACD, FAT, VAP, WTFM, PERC, DIRT) et ceux du
Tracker+ partagent visiblement le même cadre de synthé (« PolySynth »), dont le code hôte est publié
dans `tracker-ce/synth/`.

## Ce que fait le Play+ 1.3.0 aujourd'hui

Le firmware officiel du Play+ **ne charge pas de `.psyn`** : aucune trace de `psyn`, `/Synths`,
`CHIP`, `Voice Sleep` ni `PolySynth` dans le binaire (vérifié sur l'image 1.3.0). Seuls les noms des
moteurs d'usine (`DIRT`, `WTFM`…) y figurent.

## Conclusion : CHIP sur le Play+, oui, et mieux

Deux voies :

1. **Attendre une Community Edition du Play+.** Rien n'est annoncé, mais l'en-tête du SDK nomme le
   Play+. C'est une possibilité, pas une promesse.
2. **La faire nous-mêmes : un « mini chargeur `.psyn` » dans notre firmware.** On implémente
   l'interface hôte (`PsynHostApi` : allocation en SDRAM, accès aux paramètres du patch, description
   des macros) et on la branche sur les emplacements de synthé du Play+. Avantages :
   - **CHIP fonctionne tel quel**, comme **tout moteur `.psyn`** de la communauté du Tracker CE ;
   - le code est chargé depuis la carte SD en SDRAM, donc **aucune contrainte de place en flash**
     (contrairement à Braids, ~100 Ko) ;
   - nos propres moteurs, y compris un Braids adapté, peuvent suivre le même format.

C'est l'étape 2 de la feuille de route (« brancher un moteur sur une piste »), mais **avec un plan
publié par Polyend** et le code source d'un hôte cousin (`tracker-ce/synth/`). Ça devrait nettement
accélérer la cartographie du binaire du Play+.

## Découverte utile pour « alléger » : le Voice Sleep

Le Tracker CE a ajouté un réglage **Voice Sleep** : avant, les voix de synthé inactives **continuaient
d'être calculées** en permanence (« legacy … stay continuously active »). Le Play+ 1.3.0 n'a pas ce
réglage. **Hypothèse à vérifier :** une partie du coût fixe d'environ 24 % mesuré en silence viendrait
des voix de synthé inactives calculées pour rien. Si c'est confirmé, endormir ces voix serait l'un des
plus gros gains possibles.

## À faire

- [ ] Mesurer le coût de CHIP par voix (compiler le `.psyn` et l'exécuter dans le banc, comme Braids).
- [ ] Vérifier l'hypothèse Voice Sleep dans le profil du coût fixe.
- [ ] Comparer le code hôte `tracker-ce/synth/` avec le binaire du Play+ pour repérer l'hôte de synthé
      et le point de branchement des modèles.
