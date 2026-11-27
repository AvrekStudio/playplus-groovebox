# Le Play+ comme plateforme : ce qu'on sait, ce qu'il reste à mesurer

*Dossier de référence pour le grand projet (LOOM + le Play+), commencé le 27/11/2026. Il rassemble ce qui était
dispersé dans `JOURNAL.md`, `FIRMWARE_NOTES.md`, `analyses/*.md` et nos tests. À tenir à jour à chaque découverte.
Chaque point porte son niveau de confiance :* **[M]** *vérifié sur la machine,* **[V]** *vérifié sur le Play+
virtuel seulement,* **[H]** *hypothèse, à confirmer.*

## 0. Décisions pour le grand projet (le propriétaire)

- **27/11 : base = LOOM** (probablement, à confirmer), fusionnée avec ce que le Play+ a de physique (grille 128 pads
  RGB, 15 potards, écran couleur), étape par étape.
- **27/11 : aucune rétrocompatibilité**, avec quoi que ce soit : ni projets LOOM, ni projets Polyend, ni rien d'autre.
  Formats de projets, presets, structures internes : conçus pour le Play+ seulement.
- **27/11 : pas de MIDI (entrée ni sortie) dans un premier temps** : la partie MIDI est laissée de côté.
- Idées retenues : deux moteurs de samples (en mémoire / en stream depuis la carte), un éditeur d'onde façon Digitakt
  (marqueurs, boucles, découpage vers la grille de pads), accords sur la grille, arpèges sur plusieurs pistes.

## Le banc d'essai (guest/banc)

`BANC.pgi` (construit par `guest/banc/build_banc.py`, Avrek OS 0.6.2 requis pour tout) mesure sur la machine les
points « À mesurer » ci-dessous et écrit `/Guests/BANC/rapport.txt`. Déroulé : processeur et mémoire, son au repos,
son + écran, carte SD (automatiques, ~30 s), puis quatre gestes à faire (PLAY pour passer à la suite) : un potard
d'une butée à l'autre, des pads doucement puis fort, effleurer des potards, un tour d'encodeur. Le son est muet.
Test au virtuel : `vdev_own_banc` (les chiffres du virtuel ne valent rien).

## 1. Processeur

- NXP **i.MX RT1062**, un seul cœur **Cortex-M7 à 600 MHz** [M], unité flottante simple et double précision
  (FPv5-D16) [M : LOOM, CoralFlower et les autres invités y calculent en flottant]. Exécute jusqu'à 2 instructions
  par cycle ; caches d'instructions et de données de 32 Ko chacun (données du constructeur, [H] pour leur réglage par
  Polyend).
- Compteur de cycles DWT (`0xE0001004`) actif dès le démarrage de l'officiel [M]. 1 bloc audio = 1 741 497 cycles.
- Comparaison : FM-1 = 2 cœurs à 360 MHz ; Digitakt MK1 = ColdFire MCF54415 à ~250 MHz.
- **Charges : le virtuel sous-estime d'un facteur ~2** (pas de cache ni d'attente mémoire) [M, 26/11].

## 2. Mémoire

| Zone | Adresse | Taille | Vitesse | Usage | Confiance |
|---|---|---|---|---|---|
| ITCM | `0x00000000` | (part de 512 Ko FlexRAM) | 1 cycle | code DSP rapide de l'officiel (copié depuis `0x6017625C`), ISR audio `0x00013a18` | [V] |
| DTCM | `0x20000000` | (part de FlexRAM) | 1 cycle | données et tas de l'officiel ; **non exécutable** ; `cfw_ram` 256 o à `0x20002700` | [V] |
| OCRAM | `0x20200000` | 512 Ko | rapide | vide après démarrage : zone d'invités ABI 1 (512 Ko) | [M] |
| SDRAM | `0x80000000` | **32 Mo** | lente hors cache | tas « bump » de Polyend (jamais libéré) ; mémoire des samples 27,1 Mo `0x804d900c..0x81ffee80` | [M] |
| — zone invités 2 Mo | `0x81E00000` | 2 Mo | | LOOM, SLOOP, ORBIT, KARMA, Jangada, X0X | [M] |
| — zone invités 8 Mo | `0x81800000` | 8 Mo | | Skiff, Melodee, CoralFlower, Hortator | [V] |
| Flash (XIP) | `0x60000000` | 8 Mo [H] | lente hors cache | chargeur < `0x60030000` (jamais touché), application `0x60030000..0x60190a64`, notre cave `0x6018CA40` (16 Ko) | [M] |

- Les invités en SDRAM tournent bien (Auto-test : MPU ok, exécution OCRAM ok, SDRAM ok) [M].
- Toute la mémoire des samples est à nous tant qu'Avrek OS a la main (l'officiel n'y touche plus) [M, 0.6.1].
- **À mesurer** : débit et latence SDRAM avec et sans cache ; coût d'exécution du code depuis la SDRAM par rapport
  à l'OCRAM (sur un moteur réel, c'est peut-être le plus gros levier de performance du projet).

## 3. Démarrage et prise de main

- Le chargeur de Polyend installe un `.ppf` (Intel HEX) depuis la carte ; secours : bouton reset caché à côté de
  l'USB, `.ppf` pris dans `/Firmware` (**n'y laisser que des officiels**) [M : documenté, procédure connue].
- L'officiel initialise tout (horloges, SDRAM, carte, écran, codec, pilotes, **projet et samples**), puis Avrek OS
  prend la main au premier tour de sa boucle principale (`0x600ad852`, crochet en `0x600ad854`), ~200 ms [M].
- Ce qui tourne encore de Polyend : interruptions (minuteur des ms `fw_millis`, audio, LED, écran, USB [H]) et les
  pilotes appelés par nous à chaque tour (`fw_bg_services` = contrôleurs des pads, `fw_hw_update` = entrées,
  `fw_bg_service2`) [M]. Ni interface, ni séquenceur, ni moteur sonore.
- Rendre la main : à chaud si les samples sont intacts, sinon `!` dans `/Guests/boot.txt` + redémarrage [V, 0.6.1].

## 4. Son

- Chaîne : interruption de fin de DMA audio (IRQ 0, ISR ITCM `0x00013a18`) → pointeur `fw_audio_render_cb`
  (`0x2000301c`) → notre rendu → **2 tampons int16 de 128 trames (G, D), 44,1 kHz**, double tampon [M].
- Délai à tenir : **2,902 ms par bloc**, rendu compris. Dépassement = craquement (LOOM à PIC 140 %) [M].
- **L'appel audio part jusqu'à ~0,6 ms en retard** (CoralFlower, 27/11 : I = 3,5 ms) [M] : source inconnue (autres
  interruptions, sections masquées de l'hôte ?). Conséquence : viser **≤ 75 %** de rendu par bloc, ou rendre en
  tâche de fond avec un bloc d'avance (« étape 2 », à faire).
- Codec **SGTL5000** (I2C) : volume casque registre 0x22 (0 = +12 dB, 0x18 = 0 dB, 0,5 dB par pas) ; Avrek OS met
  +6 dB au chargement d'un invité [M].
- **À mesurer** : gigue de l'appel audio (distribution, pas seulement le maximum) et ses causes ; entrées audio
  (micro / ligne) : existent-elles, comment les lire [H] ; sortie USB audio.

## 5. Écran

- Puce **EVE (FT81x)** en SPI (LPSPI3, `0x4039C000`), 320 × 240, listes d'affichage, bitmaps, dégradés [M].
- Les invités dessinent 240 × 240 en RGB565 ; l'hôte envoie **seulement les rangées changées** (empreinte par
  rangée) dans la RAM_G à `0xE2000`, affichée au centre, bandes de 40 px de chaque côté pour l'hôte [M].
- RAM_G occupée par Polyend : polices et images `0..0x58000`, caches de widgets `0x7a120..0xde120` [V].
- Envoi par eDMA (canal 7, DMAMUX 16) en tâche de fond, activé par `/Guests/dma.on` : fluide sur LOOM et
  Vector37 [M]. Sans DMA : envoi bloquant, 2 ms au plus par tour de boucle.
- Ce qu'on n'exploite pas encore : l'EVE dessine elle-même (rectangles, textes, dégradés, rotations, mise à
  l'échelle de bitmaps, plusieurs couches) **sans coût processeur**. Une interface native EVE (au lieu de pixels
  dessinés par le processeur puis envoyés) libérerait le processeur et permettrait les animations façon OP-1.
- **À mesurer** : fréquence du SPI, débit réel (images pleines par seconde), modèle exact de la puce (FT812/813 ou
  BT81x : change ce qui est possible : compression ASTC, polices).

## 6. Pads et LED

- **Grille 16 × 8 + 4 colonnes de côté** : 160 pads, index = rangée × 20 + colonne ; colonnes 0–15 = pas,
  16–19 = pads de côté [M].
- Événements : appui (0), relâchement (1), tenu (2). **Pas de vélocité connue** : le pilote de Polyend passe un 3e
  argument encore jamais regardé [H : à vérifier, peut-être une pression].
- Balayage via GPIO2 / GPIO3 depuis `fw_bg_services` (contrôleurs I2C ?) [V / H].
- **LED RGB** : une trame de 7 728 o (161 × 48) par LPSPI4 + eDMA ; vert, rouge, bleu sur 16 o chacun ; pilote en
  `0x20004d84`, encodeur `0x600af998(pilote, i, r, g, b)` [M]. La 161e LED est inconnue.
- **À mesurer** : latence appui → événement ; cadence de rafraîchissement des LED ; profondeur de couleur réelle
  (8 bits par canal ?) ; vélocité ou pression (3e argument).

## 7. Potards, encodeur, boutons

- **15 potards** : lus par ADC1 (canaux 13 puis 5, 16 lectures moyennées) via un multiplexeur (GPIO2 16/17/24/25)
  [V] ; arrivent en crans relatifs (delta) ; **toucher** détecté (événement « touch », ignoré par Avrek OS) [M].
- **Encodeur** de l'écran : décodeur matériel ENC1 [V], avec clic (B6) [M].
- **Boutons** : B0 et B3 à gauche de l'écran, B6 clic encodeur, B7 Back, B8 Play, B9, B1/B2/B4/B5/B11 (Fill,
  Patterns, Save, Copy, View), B10 Shift, B12 interrupteur d'alimentation (arrière) [M].
- **À mesurer** : nombre de crans par tour complet et bruit des potards (absolus ou sans fin ?) ; utilité du
  toucher (montrer le paramètre dès qu'on effleure un potard, comme Polyend).

## 8. Carte SD et fichiers

- FatFs de l'officiel (`fw_file_open/read/write/close`, `fw_f_mkdir`, `fw_f_opendir/readdir`) ; les invités ont
  `file_read` / `file_write` d'un bloc dans `/Guests/<NOM>/` [M].
- Une écriture de 1 Mo bloque la boucle plusieurs secondes [M, CoralFlower] → écrire par morceaux (32 Ko) [V].
- **À mesurer** (indispensable pour la lecture des samples depuis la carte) : débit de lecture séquentielle,
  latence d'une petite lecture, **pires temps d'attente**, effet d'une écriture en cours ; puis une API de lecture
  partielle (décalage + longueur) qui ne bloque pas la boucle.

## 9. USB, MIDI, alimentation

- USB-C : alimentation, mise à jour, MIDI et audio USB pour l'officiel [M : manuel] ; rien d'exploité par Avrek OS
  (`0x600b0f90`, `0x600aacf8` peut-être USB / MIDI) [H].
- MIDI jack (entrée / sortie) [M : manuel], pilotes non localisés [H].
- Extinction : interrupteur tenu 1,5 s → `fw_power_off` `0x600aaa0d` [M]. Batterie : niveau non lu [H].

## 10. Ce qu'Avrek OS offre aux firmwares invités (PGI)

- En-tête `pgi_header` (magic `PGI1`, ABI 4) ; zone OCRAM 512 Ko, SDRAM 2 Mo ou 8 Mo ; `.bss` mis à zéro ; MPU et
  caches réglés par le chargeur [M].
- Services : `millis`, `led(i, r, g, b)`, `screen(fb, y0, y1, swap)`, `file_read` / `file_write`, bandes
  latérales (16 textes) [M].
- Événements : PAD, BUTTON (B10 = Shift), KNOB (delta), ENCODER [M].
- Changer d'invité à chaud (Shift + clic 2 s) [V].

## 11. Ce qui manque encore pour exploiter la machine au maximum

Par ordre d'importance pour le grand projet :

1. **Banc de mesure sur la machine** (points « À mesurer » ci-dessus) : chiffres réels au lieu d'estimations.
2. **Son en tâche de fond avec un bloc d'avance** : plus de craquements dus aux retards, toute la puissance utilisable.
3. **Lecture de fichier partielle sans bloquer** : base du moteur de samples en stream.
4. **Interface native EVE** : animations sans coût processeur.
5. **Vélocité / pression des pads**, toucher des potards.
6. ~~MIDI (jack et USB)~~ : écarté pour l'instant (décision du 27/11).
