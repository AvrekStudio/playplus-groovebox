#!/usr/bin/env bash
# Prépare l'environnement de dev (Linux, WSL ou macOS) : outils système, environnement Python,
# et vérification du firmware officiel. À lancer une fois depuis la racine du dépôt.
set -euo pipefail
cd "$(dirname "$0")/.."

PLUS_SHA=535a903dffea93b9e624c7d0f613823812f7f7b1df74c91e93e3e93e8de94876

echo "== Outils système"
missing=()
for t in clang python3 mkfs.fat mcopy; do command -v "$t" >/dev/null || missing+=("$t"); done
if [ ${#missing[@]} -gt 0 ]; then
  echo "Manquant : ${missing[*]}"
  if command -v apt-get >/dev/null; then
    echo "-> sudo apt-get install -y clang python3-venv mtools dosfstools"
    sudo apt-get install -y clang python3-venv mtools dosfstools
  elif command -v brew >/dev/null; then
    echo "-> brew install mtools dosfstools (clang vient avec les Xcode command line tools)"
    brew install mtools dosfstools
  else
    echo "Installe-les à la main puis relance ce script."; exit 1
  fi
fi

echo "== Environnement Python (.venv)"
[ -d .venv ] || python3 -m venv .venv
.venv/bin/pip install -q -r requirements.txt
.venv/bin/python -c "import unicorn, capstone, numpy, PIL; print('   unicorn', unicorn.__version__, 'ok')"

echo "== Firmware officiel"
if [ ! -f PlayPlus_v1.3.0.ppf ]; then
  echo "   PlayPlus_v1.3.0.ppf absent : télécharge-le sur polyend.com et pose-le à la racine du dépôt."
  echo "   (git l'ignore : il ne partira jamais sur GitHub)"
  exit 1
fi
got=$( (sha256sum PlayPlus_v1.3.0.ppf 2>/dev/null || shasum -a 256 PlayPlus_v1.3.0.ppf) | cut -d' ' -f1)
if [ "$got" != "$PLUS_SHA" ]; then
  echo "   ATTENTION : empreinte inattendue ($got). Il faut exactement le fichier officiel 1.3.0."; exit 1
fi
echo "   PlayPlus_v1.3.0.ppf ok (empreinte officielle)"
echo
echo "Prêt. Ensuite : scripts/test.sh   (build + tests sur le Play+ virtuel)"
