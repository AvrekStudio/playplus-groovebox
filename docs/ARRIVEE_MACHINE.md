# Arrivée du Play+ : procédure pas à pas

But : en une soirée, savoir si tout ce qui a été préparé sur le Play+ virtuel marche sur la vraie machine,
et relever ce que seul le vrai matériel peut dire (charge du processeur, numéros des commandes, mémoire).
Rien ici n'est irréversible : on revient au firmware officiel en le remettant dans `Firmware`.

## 0. Avant de toucher à quoi que ce soit

1. Allumer le Play+ tel qu'il arrive, faire le tour (son, écran, pads). Noter la version : *Settings*.
2. **Copier toute la carte SD d'origine** sur le PC (dossier `PlayPlus_carte_origine`) : projets d'usine,
   samples, réglages. C'est la sauvegarde.
3. Garder `PlayPlus_v1.3.0.ppf` (le firmware officiel, à la racine du dépôt) : c'est le retour arrière.
   Si la machine arrive avec une autre version que 1.3.0 : **s'arrêter là** et me le dire. Notre patch
   est fait pour la 1.3.0 exactement (le build vérifie l'empreinte du fichier).

## 1. Préparer la carte

```sh
scripts/package.sh          # -> dist/ ; SANS_LOOM=1 pour ne pas construire LOOM-8
```

- `dist/Firmware/PlayPlus_Avrek.ppf` → dossier `Firmware` de la carte. **Ce doit être le seul `.ppf`** de
  ce dossier : le reset d'urgence du Play+ charge un firmware de la carte au hasard.
- `dist/Guests/` → dossier `Guests` à la racine de la carte (demo, bip, loom).

## 2. Flasher

*Settings → Firmware*, choisir `PlayPlus_Avrek`. Après le redémarrage, *Settings* doit montrer la
version **`1.3.0-AVREK0.0.1`**. Si le Play+ ne redémarre pas normalement : reset d'urgence (manuel
Polyend) avec le `.ppf` officiel seul dans `Firmware`.

## 3. Ce qui ne change pas

Faire le tour comme au point 0 : un projet joue, les menus s'ouvrent, l'écran est normal. Dans le menu
principal, cinq lignes en plus tout en bas : **Diagnostic, Mode invite, Explorateur, Auto-test,
Voice Sleep**.

## 4. Diagnostic (charge réelle de Polyend)

*Menu → Diagnostic* : message « Audio xx.x% pic yy.y% ». Le relever :
- projet vide, à l'arrêt : ______ (le virtuel donne 23,7 %)
- projet d'usine en lecture : ______

## 5. Auto-test (ce que le virtuel ne peut pas vérifier)

*Menu → Auto-test*. Deux lignes orange en bas de l'écran :

| Test | Ce qu'il vérifie | Attendu |
|---|---|---|
| MPU | la protection mémoire de Polyend autorise l'exécution en OCRAM et en SDRAM | ok |
| Samples | la mémoire des samples est où on l'a trouvée (zone de LOOM-8 libre) | ok |
| OCRAM | un petit code s'exécute en mémoire interne (zone des invités demo, bip) | ok |
| SDRAM | idem en mémoire externe (zone de LOOM-8) | ok |

- **Si le Play+ redémarre pendant l'auto-test** : relancer *Auto-test*. Le test fautif s'affiche alors
  « PLANTE » et n'est pas relancé. Me noter lequel.
- « ECHEC » sur MPU seul n'empêche rien (notre chargeur règle lui-même la protection) : à me signaler.
- « ECHEC » sur SDRAM avec Samples ok : LOOM-8 ne pourra pas tourner tel quel.

Résultat : MPU ____ Samples ____ OCRAM ____ SDRAM ____

## 6. Explorateur (la table des commandes)

*Menu → Explorateur*. Chaque commande touchée s'affiche avec son numéro (« Bouton 8 appui »,
« Potard 4 +1 », « Pad r3 c10 (#70) »). Sortie : **Shift + Back**. Remplir :

| Commande physique | Ce que l'écran affiche |
|---|---|
| bouton Play | |
| bouton Shift | |
| bouton Menu / clic de l'encodeur | |
| bouton Back | |
| les autres boutons (un par ligne, avec leur nom imprimé) | |
| potards 1 à 15, de gauche à droite et de haut en bas | |
| encodeur de l'écran | |
| pads : coin haut gauche, coin bas droit, colonne des pads de fonction | |

Avec cette table, je règle la correspondance LOOM-8 (`guest/loom/plat_playplus.c`) et le reste.

## 7. Mode invité

*Menu → Mode invite* : la liste des invités (un pad bleu par invité). Encodeur + clic, ou le pad.

1. **demo** : chaque pad joue une note et s'allume de la couleur de sa rangée. Shift + clic de
   l'encodeur : retour à la liste.
2. **bip** : un bip par pad, pad blanc.
3. **LOOM-8** : l'interface au centre, CPU et PIC dans les bandes. Poser des pas (touches blanches =
   rangée du bas), Play. Puis un morceau chargé (plusieurs pistes, effets) : relever CPU ____ et
   PIC ____ (le FM-1 calcule LOOM-8 sur deux cœurs, le Play+ sur un seul).
4. SAVE (pad de fonction, colonne 19, 2e rangée), sortir (Shift + Back), revenir : le projet est
   toujours là (fichiers dans `Guests/LOOM-8` sur la carte).

À noter : ce qui sonne faux, grésille, lag, s'affiche mal ; photos d'écran bienvenues.

## 7 bis. Voice Sleep (gain de charge)

Projet qui utilise le synthé 1 (VAP). *Menu → Diagnostic* : noter la charge. *Menu → Voice Sleep* (message
« Voice Sleep actif »), attendre 2 s, *Diagnostic* à nouveau. Puis **écouter** : notes, accords, fins de
notes (relâchement), rien ne doit manquer ni craquer. Remettre inactif pour comparer à l'oreille.

Charge : sans ____ avec ____ ; à l'oreille : identique / différence : ______

## 8. Revenir au firmware officiel (à tout moment)

Mettre `PlayPlus_v1.3.0.ppf` **seul** dans `Firmware`, *Settings → Firmware*. Les projets Polyend ne
sont pas touchés par notre firmware ; le dossier `Guests` peut rester.

## 9. Me rapporter

Les valeurs des points 4, 5, 7, 7 bis, la table du point 6, et tout ce qui a surpris.
