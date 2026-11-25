#!/usr/bin/env bash
# Prépare dans dist/ tout ce qu'il faut copier sur la carte SD du Play+ (voir docs/ARRIVEE_MACHINE.md) :
#   dist/Firmware/PlayPlus_Avrek.ppf   notre firmware (patch du 1.3.0 officiel, à la racine du dépôt)
#   dist/Guests/demo.pgi, bip.pgi      invités d'exemple
#   dist/Guests/loom.pgi               LOOM-8, si son clone est là (ou SANS_LOOM=1 pour s'en passer)
# dist/ n'est jamais versionné ni publié : il contient du firmware Polyend patché et le code de LOOM-8.
set -euo pipefail
cd "$(dirname "$0")/.."
PY=.venv/bin/python
rm -rf dist && mkdir -p dist/Firmware dist/Guests
CC=clang $PY -c "import sys; sys.path.insert(0, 'cfw/tools'); import build; build.build('cfw/targets/playplus-1.3.0/target.json')" \
  | grep -E "Result" | tee /dev/stderr | grep -q "PASS"
cp cfw/build/playplus-1.3.0/PlayPlus_Avrek.ppf dist/Firmware/
python3 guest/build.py dist/Guests/demo.pgi guest/demo/demo.c
python3 guest/build.py dist/Guests/bip.pgi guest/bip/bip.c
if [ "${SANS_LOOM:-0}" != 1 ]; then
  python3 guest/loom/build_loom.py dist/Guests/loom.pgi
fi
VERSION=$(grep -o '"[^"]*"' cfw/targets/playplus-1.3.0/target.h | tr -d '"')
cat > dist/LISEZMOI.txt <<T
Firmware AvrekStudio pour Polyend Play+ — version $VERSION — $(date +%d/%m/%Y)
Usage personnel uniquement : ne pas partager ces fichiers.

Sur la carte SD du Play+ :
  Firmware/PlayPlus_Avrek.ppf  -> dossier Firmware (le SEUL .ppf de ce dossier)
  Guests/*.pgi                 -> dossier Guests (à créer à la racine)
Procédure complète : docs/ARRIVEE_MACHINE.md du dépôt.
T
echo; echo "Prêt : $(pwd)/dist"; find dist -type f | sort
