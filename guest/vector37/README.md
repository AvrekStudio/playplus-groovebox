# Vector37 sur le Play+ (Avrek OS)

Vector37 (genkigenki/Vector37, GPL-3.0, synthé « orbite » du M-Vave SMK-37) en invité PGI, **sans modifier
son code** : `plat_vector37.c` inclut son panneau simulé `tests/panel.h` (moteur, interface, écran, les
mêmes sources que sur le clavier) et le relie au Play+ (son sous interruption, interface dans la boucle,
écran 240 × 240, LED, mémoire USER dans `/Guests/VECTOR37/store.bin`).

- Construire : `guest/vector37/build_vector37.py` (clone dans `~/src/Vector37`, commit fixé).
- Matrice : `docs/correspondance/vector37.json` (page de câblage, firmware « Vector37 (SMK-37) ») ;
  `default_map.py` a fait la première proposition. Contrôles : `BTN0..17`, `ENC0..7` (K1..K8),
  `PAD0..15`, `KEY0..36`, `FADER0..3`, `WHEEL_PITCH`, `WHEEL_MOD`.
- Sur la carte : `/Guests/VECTOR37.pgi`, puis le menu d'allumage.
- Usage personnel : ne pas distribuer le `.pgi`.
