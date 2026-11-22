# Analyse : pourquoi on ne peut pas régler la longueur de note (Note Length) sur le Play+

*Analyse du 23/11/2026, firmware officiel Play+ 1.3.0, faite sur le Play+ virtuel et sur le binaire.
Rien n'a encore été vérifié sur la vraie machine.*

## En une phrase

La longueur de note existe pour les pistes **MIDI / synthé**, mais pas pour les pistes **audio
(samples)**. Ce n'est pas un réglage caché : la lecture d'un sample sur le Play+ est conçue en
**« one shot »**. Un sample joue de son Start jusqu'à son End, et la notion de « fin de note » n'existe
nulle part dans son moteur.

## Ce qu'on a constaté

### 1. Le potard n'existe pas en mode audio

Le firmware a deux tables d'étiquettes pour ses 15 potards, une par mode. Sur le **potard 6**
(rangée du milieu) :

| Mode | Fonction principale | Avec Shift | Table dans le firmware |
|---|---|---|---|
| Audio (samples) | Volume | Panning | `0x6007ac28` |
| MIDI / synthé | Velocity | **Note Length** | `0x6007abb0` |

En mode audio, il n'y a aucune entrée « Note Length », ni sur ce potard ni sur un autre. Les réglages
de forme qui existent sont *Sample Start / End* et *Sample Attack / Decay*. La seule chaîne
« Note Length » du firmware est référencée par la table MIDI et par la construction des pages de
potards du mode MIDI (vers `0x600d7038`).

Côté MIDI et synthé, la longueur va de 0,25 à 64 pas, réglée avec le potard Velocity / Note Length.

### 2. Les pas audio et MIDI ont la même structure

Sur le Play+ virtuel, un pas de piste audio et un pas de piste MIDI ou synthé ont la même taille
(0x40 octets) et les mêmes 21 paramètres (indices 0 à 20) ; seule leur classe C++ (vtable) diffère.
Les paramètres 0 à 15 des pas audio sont connus : volume, pan, filtre… (voir
`docs/FIRMWARE_NOTES.md`). On n'a **pas encore identifié** les indices 16 à 20, ni l'indice exact qui
porte la Note Length sur un pas MIDI. Le plus probable : la même case que Pan sur un pas audio, puisque
c'est le même potard en Shift. **À vérifier** avant de s'en servir.

### 3. La voix sample n'a pas de « note off »

C'est la vraie raison. D'après les notes de rétro-ingénierie reprises de FreePlay et le code de la
voix :
- chaque piste audio a **une seule voix**, qui lit le sample de son Start à son End ;
- **il n'y a pas de note off** : la voix s'arrête quand elle atteint End, ou quand un nouveau pas
  redéclenche la même piste. Même Stop ne la coupe pas ;
- l'*Attack* et le *Decay* ne sont pas des durées : ce sont des **portions de la zone jouée**
  (0 à 10000), appliquées en fondu au début et à la fin de cette zone ;
- le son est calculé par blocs de 128 échantillons (2,9 ms). Le gain de chaque bloc est fixé par une
  fonction du lecteur (`fw_reader_gain`, `0x60112b75`) à partir de ces fondus.

Pour faire une Note Length, il faudrait qu'un événement « fin de note » arrive au milieu de la
lecture et lance un fondu de sortie. Ce chemin n'existe pas dans le moteur sample. Côté MIDI et synthé,
le séquenceur, lui, programme bien une note off après la longueur choisie.

**Conclusion :** ce n'est pas une limite du processeur ni un bug, mais un choix de conception
(samples en one shot), comme le confirment les demandes d'utilisateurs sur le forum Polyend
(« Note length for Audio Mode steps », « New Playback Modes: Gate & Virtual Slice »). Polyend ne l'a
pas ajouté à ce jour.

## Peut-on l'ajouter dans notre firmware ? Oui, a priori

Les briques existent :

1. **Couper au bon moment.** On sait déjà intervenir bloc par bloc sur le gain de la voix sample
   (`fw_reader_gain`, déjà détourné avec succès par FreePlay pour son Sustain / Release). Un « gate »
   revient à : au déclenchement du pas, noter l'heure et la longueur voulue, puis, une fois la longueur
   écoulée, forcer un fondu de sortie court (quelques blocs, pour éviter les clics) et arrêter la voix.
   Précision : un bloc, soit 2,9 ms.
2. **Savoir quand une note démarre.** Le départ de note d'une piste audio passe par un appel qu'on peut
   détourner (FreePlay le fait pour ses LFO : `cfw_tgt_note`, quatre appels `bl` vers `0x6010ddd8`).
3. **Stocker la longueur par pas.** Deux pistes :
   - une case libre dans le pas, si l'un des indices 16 à 20 est inutilisé sur les pistes audio
     (à vérifier) ; l'avantage serait que la valeur est sauvegardée avec le projet par l'officiel ;
   - sinon, notre propre table (pas × pistes × patterns) en SDRAM, sauvegardée dans notre propre
     fichier à côté du projet. FreePlay fait ce genre de fichier pour ses réglages.
4. **Le régler.** Par exemple *Shift + potard Attack / Decay*, ou comme sur les pistes MIDI : maintenir
   un pas et toucher le pas de fin. À décider avec le propriétaire.

**Points d'attention :**
- comportement en cas de nouvelle note sur la même piste avant la fin (la voix est unique : c'est déjà
  un « choke » naturel) ;
- interaction avec Decay (fondu de fin de zone) et Repeat / ratchets ;
- un mode « Gate » optionnel par piste, pour que les projets existants sonnent exactement pareil.

## Prochaines vérifications (sur le virtuel)

- [ ] Identifier les paramètres 16 à 20 d'un pas (audio et MIDI), et l'indice de la Note Length MIDI.
- [ ] Repérer précisément l'appel de départ de note d'une piste audio et l'objet voix correspondant.
- [ ] Prototype : couper une voix sample au bout de N pas avec un fondu de 3 blocs, test vdev qui
      vérifie le gain bloc par bloc.
