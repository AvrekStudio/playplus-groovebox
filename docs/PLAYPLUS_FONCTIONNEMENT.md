# Le Play+ : comment il fonctionne (fiche de référence du projet)

*Synthèse rédigée à partir du manuel officiel « Play+ Manual Rev 2 » (OS 1.3.0, novembre 2025,
auteur Synthdawg pour Polyend) et d'une photo du produit, le 24/11/2026. Ce n'est pas une copie du
manuel, qui reste la référence (et n'est pas dans ce dépôt) : c'est ce qu'il faut savoir pour coder
dessus. Les numéros de page renvoient au manuel. Les sections « Pour nous » sont nos propres
remarques.*

## 1. La machine

**Face avant**, de gauche à droite :

- **Écran** couleur de 2,83 pouces en 320×240, en haut à gauche. Deux boutons à sa gauche, qui
  servent à choisir le paramètre du haut ou du bas, ou de commande contextuelle. Sous l'écran, trois
  boutons dont le libellé est affiché à l'écran ; celui du milieu est **Play/Stop** (orange). En bas à
  gauche, l'**encodeur Screen**, cranté et cliquable, **non tactile** : navigation, menu principal,
  réglage du paramètre affiché.
- **15 potards tactiles** en 3 rangées de 5. Chacun porte deux paramètres : le principal, sélectionné
  en touchant le potard, et le secondaire, sélectionné par **double tap**. Ce comportement est
  réglable. Shift + rotation donne un pas plus gros. Ce ne sont pas des encodeurs crantés.
- **6 boutons de fonction** à droite (fonction principale / avec Shift) : Fill/Delete,
  Patterns/Audio-MIDI, Save/Reset, Copy/Paste, View/Master FX, Shift.
- **Grille de 8 × 20 pads** en silicone, RGB, **non sensibles à la vélocité** : 16 colonnes de pas et
  4 colonnes de fonctions (Mute, Solo, Variation, Select). Avec Shift, ces 4 colonnes deviennent les
  4 pages de 16 pas.

**À l'arrière** : sortie stéréo 3,5 mm (casque et ligne), MIDI in/out en jack 3,5 mm (type B),
microSD, USB-C (alimentation, MIDI, audio USB, stockage), bouton reset, bouton marche/arrêt.
Alimentation 5 V / 1 A, environ 0,5 A en marche (p. 15–17, 244).

**Disposition des potards** (principal / secondaire, mode audio) :

| | Col. 1 | Col. 2 | Col. 3 | Col. 4 | Col. 5 |
|---|---|---|---|---|---|
| **Rangée 1** | Master Volume / Limiter | Note / Microtune | Sample / Folder | Reverb Send / Delay Send | Chance / Action |
| **Rangée 2** | Tempo / Swing | Volume / Panning | Sample Start / Sample End | Overdrive / Bit Depth | Randomize / Randomize Type |
| **Rangée 3** | Track Length / Play Mode | Filter Cutoff / Resonance | Sample Attack / Sample Decay | Repeat Type / Repeat Grid | Move / Micromove |

> **Pour nous :** les potards sont tactiles. Un potard **touché** est une information
> utilisable (« l'utilisateur s'intéresse à ce paramètre ») : parfait pour déclencher une animation
> avant même qu'il tourne. La grille fait 8×20, pas 8×16.

## 2. L'organisation : projet → pattern → piste → pas

- **Projet** : le conteneur d'un morceau, sur la carte SD. Sauvegarde automatique à chaque Stop.
  **128 patterns** maximum (p. 30, 32, 40).
- **Pattern** : la boucle jouée. Elle contient **16 pistes : 8 pistes audio (samples) et 8 pistes
  MIDI / synthé**. La grille montre 8 pistes à la fois : le bouton **Audio/MIDI** (Shift + Patterns)
  bascule entre les deux groupes. Les pads de fonction sont **verts** en mode audio et **violets** en
  mode MIDI / synthé (p. 42, 90).
- **Piste** : jusqu'à **64 pas** (4 pages de 16). Chaque piste a sa propre longueur (polyrythmie),
  sa vitesse (Track Speed), son swing et son **mode de lecture parmi 35** (avant, arrière, aléatoire…)
  (p. 44, 53, 63).
- **Variations** : 16 versions alternatives de chaque piste, sélectionnées à la main. Elles ne se
  chaînent pas automatiquement (p. 168).
- **Pas** : une note et tous ses paramètres. On règle d'abord un **« work step »**, un pas modèle,
  puis on le pose sur la grille. On édite un pas existant en le maintenant et en tournant un potard
  (p. 30, 128).
- **Chaîne de patterns** : arrangement linéaire, lancé par Shift + Play (p. 166).

**Couleurs de la grille** : un pas sample actif est blanc. Un pas synthé est violet (synthé 1), jaune
(synthé 2) ou vert (synthé 3). Le pas sélectionné est bleu, la tête de lecture orange (rouge en
enregistrement live). En piano roll, la note racine est rose (p. 41, 92).

## 3. Le moteur audio (p. 32, 188)

- **Samples** : un « sample pool » par projet, chargé en RAM depuis la carte SD. **255 samples** et
  **20 dossiers** maximum. Durée maximale : 6 min en mono, 3 min en stéréo, à 44,1 kHz / 16 bits.
  Les WAV sont convertis au format interne au chargement.
- **Chaîne d'une piste** (audio et synthé) : attaque / volume / decay → bit depth → overdrive →
  filtre « DJ » (passe-bas ou passe-haut) avec résonance → panoramique → envois reverb et delay
  (réglables par pas) → mixeur (mute / solo).
- **Sortie** : limiteur (avec sidechain sur une piste au choix) → saturation (hard clip, soft clip,
  saturate, overdrive) → « Sound » (EQ 5 bandes, bass boost, space).
- Reverb, delay, Sound, limiteur et saturation se règlent par **presets** (Shift + Master FX), plus un
  preset « Custom » pour certains.
- **Audio USB** : 14 canaux stéréo (sortie générale, chacune des 8 pistes audio, la somme des
  synthés). Le manuel déconseille de composer et de faire du sound design avec l'audio USB actif, car
  il peut y avoir des problèmes de lecture (p. 230).

> **Pour nous :** les effets de sortie et les envois tournent en continu, ce qui est cohérent avec le
> coût fixe d'environ 24 % mesuré sur le simulateur, même en silence (voir
> `analyses/charge-audio.md`). L'avertissement sur l'audio USB suggère que le CPU est déjà serré quand
> tout est actif.

## 4. Les synthés (p. 90–117)

- **3 emplacements de synthé** (Synth 1, 2, 3). À chacun, on assigne **un modèle parmi 6** :
  - **ACD** : monophonique, façon acid ;
  - **FAT** : 3 oscillateurs analogiques virtuels, filtres en échelle ;
  - **VAP** : polyphonique, 2 oscillateurs, matrice de modulation ;
  - **WTFM** : FM à 2 opérateurs sur tables d'onde ;
  - **PERC** : boîte à rythmes, 1 voix mais 5 sons simultanés ;
  - **DIRT** : waveshaping, ajouté en 1.3.0.
- **Polyphonie : 8 voix au total pour les 3 synthés**, réparties à la main (par exemple 3 + 3 + 2).
- Chaque **patch** définit **6 macros**, qui prennent la place de certains potards (Filter Cutoff,
  Resonance, Attack, Decay, Overdrive, Bit Depth, plus Volume en vélocité). Patches dans
  `/Patches/<MODÈLE>/` sur la carte SD.
- L'**éditeur de patch** (menu Synths) affiche jusqu'à 9 paramètres par page, réglés par les 9 potards
  centraux. Les enveloppes ADSR et LFO des synthés ont des vrais temps en secondes et un vrai
  **sustain / release** (contrairement aux samples).
- Sur une piste MIDI / synthé, le potard **Sample** choisit l'**appareil** (port MIDI et canal, ou
  Synth 1/2/3) et **Folder** choisit le patch. L'appareil peut changer **à chaque pas**.

> **Pour nous :** le plafond de **8 voix de synthé** imposé par Polyend est un indice fort sur la
> marge CPU réelle. Il faudra en tenir compte pour nos propres moteurs (voir `analyses/charge-audio.md`).
> L'architecture « 3 emplacements × N modèles + patches + 6 macros » est un bon modèle pour brancher
> nos moteurs : un nouveau « modèle » dans la liste serait le plus naturel pour l'utilisateur.

## 5. Le mode MIDI / synthé : ce qui change sur les potards (p. 65, 212)

| Potard | Mode audio | Mode MIDI / synthé |
|---|---|---|
| Note / Microtune | note / cents | note / **pitch bend** |
| Volume / Panning | volume / pan | **vélocité / Note Length** |
| Sample / Folder | sample / dossier | **appareil** / patch ou program change |
| Sample Start | début du sample | **accord** (Chord) |
| Filtre, Attack/Decay, Overdrive/Bit Depth | réglages audio | **CC MIDI** ou **macros** du synthé |
| Reverb / Delay Send | envois | CC MIDI |

- **Note Length** : en pas ou fractions de pas. L'aperçu sonore en tournant le potard ne marche pas
  pour la longueur. Les notes MIDI peuvent se chevaucher (pistes polyphoniques, accords). En « MIDI
  thru », la longueur est ignorée.
- **Le mode Perform n'existe qu'en mode audio** (p. 182).

## 6. Le séquenceur (chap. 6–7)

- **Pose et édition** : toucher un pad pour poser le work step, toucher à nouveau pour l'effacer,
  maintenir le pad et tourner un potard pour éditer. Shift + deux pads sélectionnent un rectangle de
  pas ; Shift + plusieurs pads font une sélection multiple. Une sélection de plusieurs pistes donne
  des modifications **relatives** (affichées « ~ »).
- **Copier / coller** : maintenir la source jusqu'à ce qu'elle clignote, puis toucher la destination.
  Cela marche pour les pas, les pages, les pistes et les patterns.
- **16 niveaux d'annulation** (Shift + Undo / Redo).
- **Enregistrement live** (quantifié ou non, overdub) ; **View** (les 4 colonnes de fonction
  deviennent un petit clavier) ; **piano roll** (maintenir View) ; **gammes** (filtre, type, note
  racine).
- **Fill** : remplissage algorithmique (rythmes, euclidien, mélodies, accords, motifs personnalisés
  stockés sur la carte SD).
- **Variations du jeu** : **Chance** (probabilité, ou jouer X / sauter Y…), **Action** (randomiser un
  paramètre, humaniser…), **Randomize** (sample du dossier, octave, volume, texture… avec un dosage),
  **Repeat** (ratchets : 16 styles × 17 densités), **Move / Micromove** (décalage d'un pas ou micro).
- **Perform** : la grille devient 8 catégories × 16 effets punch-in (accordage, filtre, distorsion,
  réarrangement, répétition, delay, reverb, loop). Toucher un pad verrouille l'effet ; le maintenir
  l'applique tant qu'on appuie. Il faut sélectionner des pistes, sauf pour Loop.

> **Pour nous :** le séquenceur est déjà très riche ; on le garde. Ce qui manque et qu'on peut
> apporter : la **Note Length des samples** (voir `analyses/note-length.md`), des **LFO par piste**,
> des **pages visuelles** à nous. Les gestes existants (maintenir un pad + potard, Shift + pad,
> double tap) sont la grammaire à respecter pour que nos ajouts paraissent naturels.

## 7. Carte SD et système (p. 33, 235–243)

- Dossiers : `Projects`, `Samples`, `Sample Packs`, `Artist Samples`, `BeatFills`, `MelodyFills`,
  `Patches/<modèle>`, `Workspace`, `Firmware`, `Export`. La carte doit être en **FAT32 / MBR** et
  rester en place : le Play+ ne fonctionne pas sans elle.
- **Mode stockage USB** (depuis 1.3.0) : la carte est accessible depuis l'ordinateur sans la sortir.
- **Mise à jour** : `.ppf` dans `Firmware`, puis *Settings → Firmware*. Le **reset d'urgence** charge
  un firmware de la carte au hasard : il ne faut en laisser qu'un.
- Réglages : disposition du clavier View, luminosité de la grille, double tap, aperçu en tournant
  les potards, page de variations, quantification et overdub de l'enregistrement, configuration du
  Fill.

## 8. Ce que ça change pour le projet (résumé)

1. **Note Length** : elle existe pour les pistes MIDI / synthé, sur le potard Volume en secondaire
   (double tap). Elle n'existe pas pour les samples, qui sont en « one shot ». Notre ajout devrait
   reprendre le même geste en mode audio, par exemple sur Panning ou sur un potard libre en
   secondaire. À trancher machine en main.
2. **Puissance** : Polyend limite les synthés à 8 voix au total et met en garde contre l'audio USB
   pendant la composition. Le CPU est donc déjà bien occupé par l'usine. Nos moteurs devront être
   légers, ou remplacer quelque chose plutôt que s'ajouter.
3. **Interface** : l'écran est petit (2,83 pouces). Une interface façon OP-1 y est cohérente (l'OP-1
   a aussi un petit écran), mais avec des visuels simples et lisibles. La grille 8×20 RGB est un
   deuxième écran à exploiter.
4. **Intégration des moteurs** : les brancher comme un nouveau « modèle » dans les 3 emplacements de
   synthé, avec leurs 6 macros, serait l'intégration la plus naturelle.
