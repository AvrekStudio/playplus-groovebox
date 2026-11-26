# FM1 Tonic sur le Play+ (Avrek OS)

FM1 Tonic (luigirossi-music/fm1-tonic, GPL-3.0, Luigi Rossi ; fork de Felucca / X0X) en invité PGI, **sans
modifier son code** : `plat_tonic.c` fait ce que fait son simulateur PC (`host/x0x_host.c`) — il réalise
`firmware/src/app/plat.h` et inclut l'app (`project.c`, `ui.c`) — mais contre le Play+ : son sous interruption
(`engine_render` par blocs de 256), interface dans la boucle (`ui_frame` toutes les 16 ms), écran 240 × 240,
LED des pads, mémoire en fichiers.

- Construire : `guest/tonic/build_tonic.py` (clone dans `~/src/fm1-tonic`, commit fixé `cf0b9ec`, firmware 1.8g).
- Matrice : `docs/correspondance/tonic.json`, écrite par `default_map.py` (steps en rangée 7, pistes / ACCENT /
  FILL / FUNC en rangée 6, pages en rangée 0, potards 1 MASTER, 2-5 KNOB 1-4, 6 ALGORITHM, 7 PRESETS, encodeur
  SELECT, B8 PLAY, B9 REC, B4 SAVE, B6 = OCT+ (OUI), B3 = OCT- (NON)). Mêmes noms que LOOM-8 (B_*, EN_*,
  KEY0..26, MASTER, « C:HOME+SELECT ») : tables par `guest/loom/gen_map.py`. À rouvrir dans la page de câblage
  (« Importer un .json », firmware FM1 Tonic).
- Sur la carte : `/Guests/TONIC.pgi`, et si besoin `/Guests/TONIC/map.json` (remplace la matrice compilée,
  bande « MAP SD »). Puis le menu d'allumage.
- Mémoire, dans `/Guests/TONIC/` : objets de Tonic `o00.bin`..`o06.bin` (+ `.old`, la copie précédente),
  projets enregistrés (SAVE) `pNN.bin` et leur second endroit `xNN.bin`.
- Absents : second cœur (tout sur un cœur), USB, MIDI, samples utilisateur (le FM-1 n'en a plus non plus).
- Usage personnel : ne pas distribuer le `.pgi`.
