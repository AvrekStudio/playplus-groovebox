# Feuille de route

L'objectif : faire du Play+ une groovebox personnelle, avec un écran fun et inspirant façon OP-1, des
potards qui pilotent des macros, des moteurs de son maison, sans rien perdre du séquenceur Polyend.

On avance par petites étapes. Chaque étape est testée sur le Play+ virtuel, puis validée sur la machine.

## Étape 0 : le socle (fait, à valider sur la machine)

- [x] Build, vérification et Play+ virtuel opérationnels.
- [x] Notre version et un point d'entrée dans la boucle principale.
- [ ] À la réception : mettre le Play+ en firmware officiel 1.3.0 si besoin, flasher notre build,
      vérifier la version dans *Settings → Firmware*.
- [ ] Revenir une fois au firmware officiel pour s'assurer que la porte de sortie marche.

## Étape 1 : mesurer avant de construire

Il faut d'abord savoir combien de CPU et de mémoire il reste.

- [x] Mesure de la charge audio : compteur de cycles du Cortex-M7 autour du traitement d'un bloc.
- [x] *Menu → Diagnostic* : message avec la charge audio (moyenne et pic sur 1 s).
- [ ] Une vraie page de diagnostic à nous (notre premier écran) : charge audio, durée de la boucle,
      RAM libre, rafraîchie en continu.
- [ ] Relever les chiffres à vide, avec 8 pistes de samples, puis avec 3 synthés d'usine.

## Étape 1 bis : alléger (décision du propriétaire, 24/11/2026)

Récupérer du CPU en coupant les fonctions d'usine qui ne servent pas, plutôt que de tout empiler.
Méthode : **mesurer d'abord**, couper ensuite, et toujours de façon réversible (réglage dans notre
menu, rien de définitif).

- [ ] Nommer chaque morceau du coût fixe (~24 % sur le simulateur) : reverb, delay, limiteur,
      saturation, EQ / bass boost / space, chaînes de piste, mixage, audio USB.
- [x] Hypothèse **Voice Sleep** : **confirmée** (25/11). Les voix de synthé inactives coûtent ~18 points
      sur 24 en silence ; le vrai coût fixe est d'environ 5,5 %.
- [ ] **Implémenter un Voice Sleep** : ne plus calculer une voix de synthé muette. Priorité n°1 de l'allègement.
- [ ] Vérifier lesquels coûtent même inutilisés (exemple : l'audio USB calcule-t-il ses 14 canaux sans
      ordinateur branché ?).
- [ ] Candidats cités par le propriétaire : **audio USB**, **samples en stéréo** (le mono lui suffit).
      Liste à compléter avec lui.
- [ ] Interrupteurs « Économie » dans *Menu → Diagnostic* (ou un menu à nous), avec le gain mesuré
      affiché pour chacun.

## Étape 2 : premier son maison

- [ ] Cartographier comment les moteurs synthé d'usine (ACD, FAT, VAP, WTFM, PERC, DIRT) reçoivent les
      notes et rendent leurs blocs audio.
- [ ] Brancher un oscillateur très simple (une sinusoïde, puis une dent de scie) à la place d'un moteur.
- [ ] Piste privilégiée : un **mini chargeur `.psyn`** (format officiel des moteurs du Tracker CE,
      MIT) pour jouer **CHIP** et les autres moteurs CE (voir `docs/analyses/chip-psyn.md`).
- [ ] Puis un vrai moteur : **Braids** est le premier candidat (étude `docs/analyses/braids.md` :
      23 à 220 instructions/échantillon, au moins 8 voix même au pire).

## Étape 3 : macros

- [ ] Un potard pilote plusieurs paramètres, chacun avec sa plage et sa courbe.
- [ ] Les macros sont enregistrées avec le projet.
- [ ] L'écran montre en direct ce que fait la macro.

## Étape 4 : l'écran façon OP-1

- [ ] Une scène animée par moteur, dessinée par la puce EVE : sprites, rotations, dégradés.
- [ ] Des animations qui réagissent au tempo et au son (niveau, enveloppe, pas en cours).
- [ ] Un style graphique cohérent : palette, typographie, mouvements.

## Vision : le Play+ comme plateforme pour les firmwares FM-1 (25/11/2026)

Idée du propriétaire : faire tourner les **firmwares custom du M-VAVE FM-1** sur le Play+, en
**« mode invité »** (le firmware Polyend reste en dessous comme couche matérielle). Le Play+ apporte ce
qui manque au FM-1 : un grand écran couleur, 15 potards, 160 pads, des boutons. Les firmwares FM-1
apportent les moteurs et les interfaces, déjà dessinés : plus de design à faire, l'écran du FM-1 est
simplement agrandi en plein écran.

- [x] Premier firmware : **LOOM-8** (MPL-2.0 / GPL-3.0), sur la plateforme **fm1-base** ; étude dans
      `docs/analyses/loom-portage.md`.
- [x] Mesurer la charge de LOOM sur Cortex-M7 (banc Unicorn) : chanson chargée **18 %** (25/11).
- [ ] Inventorier sa couche matérielle (écran, touches, encodeurs, audio, stockage) : c'est ce que notre
      « couche de compatibilité FM-1 → Play+ » doit fournir. Si plusieurs firmwares partagent la même
      base, une seule couche les fera tous tourner.
- [ ] Table de correspondance des commandes (FM-1 → Play+), et usage des potards en plus.
- [ ] Mise à l'échelle de l'écran (facteur entier si possible, pour rester net).
- [x] Mode invité M1 : son (rendu remplacé) et entrées (listener intercepté), entrée par le menu,
      sortie Shift + Back (`cfw/src/guest.c`, test `vdev_guest`).
- [x] Mode invité M2 : l'écran entier (`cfw/src/guest_screen.c`), Polyend le reprend à la sortie.
- [x] Mode invité M3 : LED des pads (`cfw/src/guest_led.c`), trame de Polyend remise à la sortie.
- [x] Mode invité M4 : invité PGI chargé depuis la carte SD en OCRAM (`guest_load.c`, `guest/`).
- [x] Plusieurs invités sur la carte (dossier `Guests`), sélecteur, changement à la volée (`guest_pick.c`).
- [ ] Sur la machine : vérifier l'exécution depuis l'OCRAM (MPU, caches).
- [ ] Mode invité M5 : `plat_playplus.c` pour fm1-base, puis LOOM (son, image 240 x 240, LED).

## Étape 5 : séquenceur

À définir à l'usage, une fois la machine en main.
