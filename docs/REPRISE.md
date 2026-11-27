# Reprendre le projet sans mémoire (pour une autre session de Claude)

*Écrit le 27/11/2026, à tenir à jour à chaque séance (avec `JOURNAL.md`). Si tu lis ceci sans souvenir des
échanges : tout ce qu'il faut est ici ou pointé d'ici. Lis dans l'ordre : ce fichier, `CLAUDE.md`, l'« État actuel » de
`JOURNAL.md`, puis `PLATEFORME.md` pour le grand projet.*

## Le propriétaire et la façon de travailler

- AvrekStudio, francophone : **réponds et documente en français**. Il teste sur sa vraie machine (Polyend Play+) et
  rapporte ce qu'il voit (chiffres de la bande latérale, photos). Il n'est pas développeur : explique simplement.
- Il veut le travail fait, pas des questions : agir, tester au virtuel, livrer les fichiers, puis dire quoi essayer.
- Rien ne doit se perdre : **tout passe par git** (branche → PR → fusion), le journal est mis à jour à chaque étape.

## Où sont les choses

| Quoi | Où |
|---|---|
| Le dépôt | `AvrekStudio/playplus-groovebox` (**public**). Cloner dans `~/playplus-groovebox`, puis `scripts/setup.sh` |
| Fil rouge, décisions, pièges | `docs/JOURNAL.md` (entrées datées, les plus récentes en bas ; « État actuel » en haut) |
| La plateforme (matériel, mémoire, son, écran…) et le grand projet | `docs/PLATEFORME.md` |
| Avrek OS (l'hôte qui remplace l'appli Polyend) | `cfw/src/own.c` (boucle, menu, retour au menu), `cfw/src/own_guest.c` (chargeur, services, écran, son), `cfw/src/pgi.h` (format des invités, ABI) |
| Cible et version | `cfw/targets/playplus-1.3.0-own/` (`target.h` : `CFW_VERSION_STRING`) ; image produite `cfw/build/playplus-1.3.0-own/PlayPlus_AvrekOS.ppf` |
| Les firmwares invités | `guest/` : `loom/` (LOOM-8), `vector37/`, `x0x/`, `tonic/`, la famille Felucca (`felucca_build.py`, `FELUCCA.md`), `banc/` (banc d'essai) |
| Couches communes des invités | `guest/sdk/` : `felucca_pgi.h` (son sous interruption), `webapi_pgi.h` (émulateurs web ; mode `WA_LOOP`/`WA_ISR`), `mapjson.h` (map.json), `autorep.h` (crans répétés) |
| Matrices de commandes (FM-1 → Play+) | `docs/correspondance/<firmware>.json` ; sur la carte : `/Guests/<NOM>/map.json` |
| La page de câblage (éditeur de matrices) | **artifact claude.ai** « Câblage firmware → Play+ », slug `addd99c8-0ec7-44fe-8103-32bd5e26fee4` (`read_artifact` pour la relire, `publish_files` pour la republier). **Pas dans git** (dépôt public, elle contient des dessins et captures de firmwares GPL) |
| Sources des firmwares tiers | clonés dans `~/src/<dépôt>` à des commits fixés par les scripts de build (LOOM `22d9613` + fm1-base `7337375`, X0X/Tonic `6515e77`, SLOOP `bac9c05`, ORBIT `1dbb77d`, KARMA `1d67ab9`, Jangada `b546675`, Skiff `392c1df`, Melodee `165d177a`, CoralFlower `46790b9`, Hortator `44c9ad4`). **Jamais modifiés, jamais commités** |
| Firmware officiel | `PlayPlus_v1.3.0.ppf` à la racine du dépôt (fourni par le propriétaire, **jamais dans git**) |

## Règles absolues

- Jamais de firmware Polyend dans git, jamais de `.ppf` patché publié, jamais de code ou de sortie de
  décompilateur de l'officiel recopiés (adresses et notes de comportement : oui).
- Les `.pgi` des invités (GPL / MPL) : **usage personnel**, jamais commités (`guest/.gitignore`), envoyés au
  propriétaire seulement. On ne modifie pas le code des firmwares tiers : toute adaptation va dans nos couches.
- `main` toujours vert ; le build finit sur `Result: **PASS**` ; tout nouveau comportement a un test `vdev_*`.
- Commits : fin de message avec `Co-Authored-By` et `Claude-Session` (lignes données par la session) ; PR créées et
  fusionnées par `gh api` (le GraphQL et l'écriture des étiquettes sont refusés par le proxy de la session).

## Versions (les étiquettes ne peuvent pas être poussées d'ici : les voici par commit de fusion)

| Version | Commit (main) | Statut |
|---|---|---|
| Avrek OS 0.5.3 (envoi écran eDMA réparé) | `35fc67f` (PR #38) | validé sur la machine (avec `/Guests/dma.on`) |
| Avrek OS 0.6.0 (9 firmwares Felucca, zone 8 Mo, menu 16) | `9abcb56` (PR #39) | sur la machine |
| LOOM-8 / X0X rendu 128 trames (fin des craquements) | `eb3f5e3` (PR #40) | validé sur la machine (« plus de crack ») |
| Avrek OS 0.6.1 (retour au menu à chaud, samples écrasés, Jangada, crans répétés) | `334c56a` (PR #42) | **sur la machine du propriétaire** (étiquette locale `v0.6.1-os`) |
| Invités web : écran 60 /s, CALC ; CoralFlower son sous interruption, marge x1,5, flash par morceaux | `b07cfed`..`337fa30` (PR #43 à #48) | livrés, essais en cours |
| Avrek OS 0.6.2 (ABI 5 : fopen/fread, PADX, TOUCH) + banc d'essai | branche `feat/banc-abi5` | **en cours** |

## Branches à connaître (hors `feat/...` déjà fusionnées)

- `feat/banc-abi5` : travail en cours (voir plus bas).
- `feat/navigateur-samples` et `archive/navigateur-samples-stash` : un navigateur de samples pour l'appli Polyend,
  abandonné (non testé) ; gardé pour mémoire.

## Où on en est (27/11/2026, fin d'après-midi)

- Le propriétaire utilise Avrek OS 0.6.1 avec LOOM-8 et les invités. Retours en attente :
  - **CoralFlower** : craquements à l'attaque des notes ; dernière version livrée = marge x1,5 + flash par morceaux.
    Ses bandes : CALC (interface), « M » (masquage, ms), « I » (écart entre appels audio, ms ; normal 2,9), « S »
    (voix délestées). Dernières mesures : PIC 82 %, M 0, I 3,5 → l'appel audio part jusqu'à 0,6 ms en retard (vient
    de l'hôte). Si ça craque encore : faire le **son en tâche de fond avec un bloc d'avance** dans Avrek OS.
  - Gel au changement de son (sauvegarde de 1 Mo) : corrigé (morceaux de 32 Ko), à confirmer.
  - Les autres invités (Skiff, Melodee, Hortator, Jangada, X0X…) : pas encore tous essayés.
- **Grand projet** (décisions dans `PLATEFORME.md` §0) : base LOOM (probablement), fusion avec le Play+, aucune
  rétrocompatibilité, pas de MIDI pour commencer, deux moteurs de samples (mémoire / stream), éditeur d'onde.
  Étape 1 en cours : connaître la machine → **banc d'essai** (`guest/banc`, Avrek OS 0.6.2) à faire tourner sur la
  machine, puis remplir `PLATEFORME.md` avec les vrais chiffres.

## Pièges qui coûtent cher (détails dans JOURNAL.md)

- Le Play+ virtuel : premier démarrage d'une image ~10 min ; son compteur de cycles avance **par tranches** (mesures
  de durée sans valeur, sauf `exact_count`) ; l'instantané est pris **après** la prise de main d'Avrek OS ; un fichier
  posé sur la carte après l'instantané n'est vu qu'en le reposant (`mdel` + `sd_put`).
- Les charges mesurées au virtuel sont ~2 fois trop basses.
- Les tests longs dépassent 10 min : les lancer en arrière-plan et attendre leur fin avant de répondre.
- Construire Melodee demande `fonttools` (dans `requirements.txt`).
