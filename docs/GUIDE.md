# Guide du projet

## Les deux règles à ne jamais enfreindre

1. **Aucun fichier de firmware Polyend sur GitHub.** Pas de `.ppf`, `.bin` ni `.hex`. Le `.gitignore`
   les bloque ; ne force jamais l'ajout avec `git add -f`.
2. **Ne jamais publier un `.ppf` patché.** Il contient le firmware de Polyend. Si on partage un jour le
   projet, ce sera sous forme de patch que chacun applique à son propre fichier officiel.

On ne copie pas non plus de code issu du firmware officiel ou d'un décompilateur. Les adresses et les
notes sur le comportement du firmware sont autorisées.

## Comment ça marche

- Le firmware officiel `PlayPlus_v1.3.0.ppf` est un fichier texte Intel HEX, non chiffré.
- Notre code C (`cfw/src/`) est compilé par clang pour le Cortex-M7, puis placé dans un espace libre de
  l'image : **la cave**, entre `0x6018CA40` et `0x60190A40`, soit 16 Ko. Si elle ne suffit plus, on
  pourra ajouter une **région ext** après la fin de l'image (64 Ko et plus). L'outil le gère déjà.
- Des **crochets** (« hooks ») changent un pointeur ou un appel du firmware d'origine pour qu'il passe
  par notre code. Notre code appelle en général la fonction d'origine, puis fait son propre travail. La
  liste est dans `cfw/targets/playplus-1.3.0/target.json`.
- Les adresses des fonctions de l'officiel que l'on appelle sont dans `symbols.ld`. Les décalages dans
  ses structures sont dans `target.h`.
- Le build vérifie le fichier produit : seuls les octets attendus changent, chaque crochet remplace
  exactement la valeur prévue, et chaque adresse de notre code correspond à un symbole connu.

### Le socle actuel

| Fichier | Rôle |
|---|---|
| `cfw/src/version.c` | Notre version, visible dans *Settings → Firmware* : la preuve que notre code est flashé. |
| `cfw/src/loop.c` | Point d'entrée à chaque tour de la boucle principale. Compte les tours et mesure le plus long écart entre deux tours. |
| `cfw/src/audio_load.c` | Chronomètre le calcul de chaque bloc audio (charge CPU). |
| `cfw/src/menu.c` | Nos lignes dans le menu principal (*Diagnostic*). |
| `cfw/src/diag.h` | État partagé de la mesure et **carte d'occupation de `cfw_ram`**. |
| `cfw/src/fmt.h` | Écrire des nombres en texte sans libc. |
| `cfw/src/cfw.h` | Types et petites macros d'accès aux objets du firmware. |

### Contraintes du C sur la machine

- Pas de libc : ni `printf`, ni `malloc`, ni `memcpy`. Pas d'affectation de struct (le compilateur la
  traduit en `memcpy`).
- Pas de variables globales modifiables. L'état vit dans `cfw_ram` (256 octets libres en RAM) ou, plus
  tard, dans un bloc alloué une fois au démarrage.
- Évite les gros `switch` : le vérificateur ne sait pas mesurer les tables de saut qu'ils génèrent.
  Préfère des `if` ou des tableaux.
- Le chemin audio tourne sous interruption : rapide, sans allocation ni accès fichier.

## Matériel du Play+

NXP i.MX RT1062 (Cortex-M7 à 600 MHz avec FPU), 1 Mo de RAM interne et 32 Mo de SDRAM, écran 320×240
piloté par une puce graphique EVE (listes d'affichage, bitmaps, dégradés, rotations), audio traité en
blocs de 128 échantillons. Ce qu'on sait du firmware se trouve dans
[FIRMWARE_NOTES.md](FIRMWARE_NOTES.md) (notes de FreePlay, en anglais). On y ajoutera nos découvertes.

## Installer l'environnement sur ton PC

Sous Windows, passe par **WSL (Ubuntu)**. Sous Linux ou macOS, tout fonctionne directement.

```bash
git clone https://github.com/AvrekStudio/<nom-du-dépôt>
cd <nom-du-dépôt>
# pose PlayPlus_v1.3.0.ppf (fichier officiel de polyend.com) à la racine
scripts/setup.sh
```

Le script installe ce qui manque (clang, mtools, mkfs.fat, environnement Python) et vérifie l'empreinte
de ton firmware. Il faut exactement le fichier officiel 1.3.0.

## La boucle de travail

```bash
scripts/test.sh               # build + tous les tests sur le Play+ virtuel
scripts/test.sh vdev_base     # build + un seul test
NOBUILD=1 scripts/test.sh     # relancer les tests sans recompiler
```

| Niveau | Durée | Ce qu'il vérifie |
|---|---|---|
| Build | quelques secondes | octets modifiés, crochets, adresses |
| Play+ virtuel (`cfw/tests/vdev_*.py`) | environ 10 min au premier démarrage d'une image, puis quelques secondes à quelques minutes | démarre le vrai firmware patché, appuie sur les boutons, lit la mémoire et l'écran, prend des captures |
| Vrai Play+ | — | le son, la charge CPU réelle, le vrai rendu |

Le rapport détaillé de chaque build se trouve dans `cfw/build/playplus-1.3.0/*.md`. Les captures
d'écran du Play+ virtuel sont dans `~/.cache/vplay/.../shots/`.

**Ce que le virtuel ne sait pas faire :** mesurer la charge CPU réelle, faire entendre les défauts audio,
montrer le vrai rendu des pads et de l'écran, simuler les potards physiques. Tout ce qui touche au son se
valide sur la machine.

**Mesurer sur le virtuel :** par défaut, l'émulateur ne compte pas le temps passé dans les
interruptions. `vp.exact_count(True)` compte chaque instruction (1 instruction = 1 cycle) : plus lent,
et seulement un ordre de grandeur. Pour faire tourner l'audio : `hle.enable_audio(vp)`.

### Écrire un test

Copie `cfw/tests/vdev_base.py`. Un test ouvre la machine virtuelle (`open_device`), la pilote
(`s.tap`, `s.pad`, `s.turn`, `s.knob`, `s.run_ms`), lit la mémoire (`vp.mu.mem_read`) ou appelle des
fonctions (`vp.call_fn`), prend des captures (`s.shot`), puis vérifie avec `check(nom, condition)`.

## Flasher sur le Play+

1. Sauvegarde toute la carte SD.
2. Après un build, le fichier se trouve dans `cfw/build/playplus-1.3.0/PlayPlus_Avrek.ppf`.
3. Copie-le dans le dossier `Firmware` de la carte, **avec** `PlayPlus_v1.3.0.ppf` (ta porte de
   sortie), puis éjecte la carte proprement.
4. Sur le Play+ : *Settings → Firmware → Update Firmware*, choisis notre fichier. Ne coupe pas
   pendant la mise à jour.
5. Vérifie dans *Settings → Firmware* que la version est bien la nôtre.

Si la machine ne propose pas notre fichier, c'est que son nom ne lui convient pas. Note-le-moi : on
ajustera le nom de sortie dans `target.json`. Elle accepte au moins les noms qui commencent par
`PlayPlus`.

**Revenir à l'officiel :** même chemin, en choisissant `PlayPlus_v1.3.0.ppf`.
**Si la machine ne démarre plus :** ne laisse que le fichier officiel dans `Firmware`, puis appuie sur
le petit bouton reset au dos. Le bootloader n'est jamais modifié, donc ce secours fonctionne toujours.

## GitHub : la méthode

**`main` est toujours propre** : ça compile et les tests passent. On travaille sur une branche par
fonctionnalité :

```bash
git switch main && git pull
git switch -c feat/charge-cpu
# ... modifications, scripts/test.sh ...
git add -A
git commit -m "Ajoute la mesure de charge CPU"
git push -u origin feat/charge-cpu
```

Ensuite, on ouvre une **Pull Request** de la branche vers `main` sur GitHub. Elle dit ce qui a été fait
et testé (sur le virtuel et/ou sur la machine). On la fusionne quand c'est bon. Ça donne un historique
lisible de tout le projet.

**Versions :** à chaque étape validée sur la machine, on change la version dans `target.h`
(`1.3.0-AVREK0.0.2`, etc.) et on pose une étiquette :
`git tag v0.0.2 && git push --tags`.

**Les outils de FreePlay :** ils ont été copiés au départ, on ne suit pas son dépôt automatiquement.
S'il améliore ses outils ou porte la carte des adresses vers un nouveau firmware Polyend, on reprendra
ce qui nous sert à la main.
