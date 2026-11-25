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
- [ ] Hypothèse **Voice Sleep** : les voix de synthé inactives sont-elles calculées en permanence ?
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

## Étape 5 : séquenceur

À définir à l'usage, une fois la machine en main.
