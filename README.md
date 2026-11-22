# Play+ Groovebox

Firmware personnel pour le **Polyend Play+**. Objectifs : des moteurs de son maison, des potards qui
pilotent des macros, un écran fun et inspirant façon OP-1, sans rien perdre du séquenceur Polyend.

Ce n'est ni un produit Polyend ni un projet soutenu par Polyend. C'est du code expérimental.

## Où on en est

Version **0.0.1** : le socle. Notre code est flashé dans le firmware officiel 1.3.0 et appelé à chaque
tour de la boucle principale. On le reconnaît à sa version, `1.3.0-AVREK0.0.1`, dans
*Settings → Firmware*. Aucune fonctionnalité visible pour l'instant.

Prochaines étapes : [docs/FEUILLE_DE_ROUTE.md](docs/FEUILLE_DE_ROUTE.md). Historique et décisions : [docs/JOURNAL.md](docs/JOURNAL.md).

## Comment ça marche

On ne réécrit pas le firmware : on **ajoute notre code** dans un espace libre de l'image officielle et
on **détourne quelques appels** vers lui. Le firmware Polyend n'est jamais dans ce dépôt. Chacun
télécharge son fichier officiel, et le build y applique nos modifications, vérifiées octet par octet.

Tout se teste sans la machine, sur un **Play+ virtuel** qui exécute le vrai firmware dans un émulateur.

Le guide complet (installation, build, tests, flash, retour arrière, méthode GitHub) se trouve dans
[docs/GUIDE.md](docs/GUIDE.md).

## Démarrage rapide

```bash
# pose PlayPlus_v1.3.0.ppf (téléchargé sur polyend.com) à la racine du dépôt
scripts/setup.sh      # outils et environnement Python
scripts/test.sh       # build + tests sur le Play+ virtuel
```

## Crédits

Les outils de build, le vérificateur, le Play+ virtuel et la carte des adresses du firmware 1.3.0
viennent de [FreePlay](https://github.com/sloppyjoo/freeplay) (licence MIT). Merci à son auteur pour ce
travail de rétro-ingénierie. Nos fonctionnalités sont écrites de zéro.
