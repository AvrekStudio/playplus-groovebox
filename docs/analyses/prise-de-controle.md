# Étape 1 : où prendre le contrôle du Play+, et ce qu'on garde de Polyend

*26/11/2026. Relevé au désassembleur sur l'officiel 1.3.0 et vérifié sur le Play+ virtuel. Seules les
adresses et le comportement sont notés ici (aucun code de Polyend recopié).*

## Le principe retenu

- **Le chargeur de démarrage n'est jamais touché.** On ne remplace que l'image de l'application
  (`.ppf`), par la mise à jour normale.
- L'officiel démarre comme d'habitude : horloges, SDRAM, carte SD, écran, codec, pilotes, application.
- **À l'entrée de sa boucle principale, notre code prend la main** et ne la rend plus, sauf si on
  choisit « Play+ d'origine » dans notre menu de démarrage.

## La boucle principale de l'officiel

Elle commence en **`0x600ad852`** (dans la fonction de démarrage qui précède, juste après les
initialisations). Mesure au virtuel : ~18 000 tours par seconde à vide. Un tour :

| Adresse de l'appel | Fonction | Rôle (constaté ou supposé) |
|---|---|---|
| `0x600ad854` | `0x600aba50(sb)` | vide (`bx lr`) : **notre point d'accroche idéal** (crochet « bl », rien à remplacer) |
| `0x600ad858` | `0x600b0f90()` | services en tâche de fond (files à `0x20005df8`, `0x20005e14`, `0x20006270`, `0x20003048`) : USB / MIDI probablement, à confirmer ; à garder |
| `0x600ad85e` | `0x600aab28(r7 = 0x20004c9c)` | **lecture de tout le matériel** (pads, boutons, potards, toucher, encodeur) → événements au « listener » ; appelle aussi l'automate d'affichage de Polyend en `0x600aab34` |
| `0x600ad864` | `0x600aacf8(r8 = 0x200058f0)` | autre service (drapeau en `0x20000604 + 5`) : à identifier |
| `0x600ad870` | case 1 de la table de l'application (`[0x20005a18]` = app `0x20018c18`) | **toute l'application Polyend** : interface, séquenceur, sauvegardes. **On ne l'appelle plus.** |
| — | toutes les 0,5 s : bascule de la broche GPIO3.26 ; toutes les 5 s : `0x600ae378(0x20004c90)` | battement / tâche périodique, à identifier |

## Ce qu'on garde, et comment on s'y branche

| Besoin | Pièce de Polyend gardée | Notre branchement |
|---|---|---|
| Contrôles (pads, boutons, potards, encodeur) | `0x600aab28` (balayage matériel) | À la prise de contrôle, on remplace le pointeur de table du listener (`[0x20018c34]`, d'origine `0x6007b27c`) par **notre table** : tous les événements arrivent chez nous. Le **toucher des potards** (case 3) est ignoré. |
| Écran | fonctions EVE déjà nommées (`fw_eve_*`) | On ne laisse plus tourner l'automate d'affichage de Polyend (crochet « bl » en `0x600aab34`, comme le mode invité) ; on envoie nos listes d'affichage nous-mêmes, en commandes natives. |
| LED des pads | pilote `0x20004d84`, encodeur `0x600af998`, trame LPSPI4 + eDMA | Comme `guest_led.c`. |
| Son | interruption de fin de DMA audio (ITCM `0x00013a18`), double tampon | On met **notre rendu** dans le pointeur `fw_audio_render_cb` (`0x2000301c`, d'origine `0x600ad379`) : deux tampons int16 de 128 échantillons (gauche, droite), 44,1 kHz. Plus aucun calcul audio de Polyend. |
| Volume | codec SGTL5000 (I2C, registre 0x22) | Fonctions déjà relevées (`fw_codec_hp_vol`…), si besoin. |
| Carte SD | `fw_file_*` (FatFS) | Lecture des firmwares et des réglages, depuis la boucle (jamais sous interruption). |
| MIDI / USB | `0x600b0f90`, `0x600aacf8` (à confirmer) | On continue de les appeler à chaque tour. |

## Le chemin de secours (manuel du Play, § 11.4)

Si une image ne démarre plus : **bouton reset caché à l'arrière, à côté de l'USB** (une épingle). Le
chargeur de démarrage cherche alors un `.ppf` dans `/Firmware` sur la carte et l'installe ; **il en prend
un au hasard** s'il y en a plusieurs. Règle pour nous : **dans `/Firmware`, ne garder que des `.ppf`
officiels** (1.3.0…), et n'y poser le nôtre que le temps de le flasher.

## Étapes suivantes

1. ~~Point de prise de contrôle et pilotes à garder~~ (ce document).
2. **Boucle de test** : crochet en `0x600ad854`, notre table de listener, notre rendu audio (une sinusoïde),
   écran à nous (contrôles affichés en direct), pads allumés ; mesure de la latence contrôle → écran.
3. Menu de démarrage (firmwares de la carte, « Play+ d'origine », « Mise à jour »).
4. `plat.h` du Play+ avec la matrice `docs/correspondance/loom-8.json`, puis LOOM-8.
5. Compléments par firmware.

## Points ouverts

- Identifier `0x600b0f90`, `0x600aacf8`, `0x600ae378` (USB, MIDI, autre ?).
- L'application Polyend est quand même **initialisée** au démarrage (chargement du projet) : temps de
  démarrage inchangé. Plus tard, on pourra court-circuiter cette initialisation.
- Interruptions de Polyend qui continuent (USB, MIDI, minuteries) : vérifier qu'aucune ne dépend de
  l'application qu'on n'appelle plus.
