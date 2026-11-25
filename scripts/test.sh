#!/usr/bin/env bash
# Build + tests sur le Play+ virtuel.
#
#   scripts/test.sh                 build, puis tous les tests cfw/tests/vdev_*.py
#   scripts/test.sh vdev_base       build, puis seulement ces tests
#   NOBUILD=1 scripts/test.sh ...   réutilise le dernier build
#   TARGET=playplus-1.3.0-base scripts/test.sh   la cible minimale (officiel + Knob Touch) : ses tests sont
#                                   listés dans cfw/targets/<cible>/tests.txt
#
# Le build (quelques secondes) vérifie chaque octet modifié et chaque crochet. Les tests démarrent
# ensuite le vrai firmware patché dans le Play+ virtuel : ~10 min la première fois pour une image
# donnée, puis départ depuis un instantané (~/.cache/vplay/).
set -uo pipefail
cd "$(dirname "$0")/.."
PY=.venv/bin/python
TARGET=${TARGET:-playplus-1.3.0}
export TARGET
OUT=cfw/build/$TARGET

if [ $# -gt 0 ]; then TESTS="$*"
elif [ -f cfw/targets/$TARGET/tests.txt ]; then TESTS=$(grep -v '^#' cfw/targets/$TARGET/tests.txt)
else TESTS=$(cd cfw/tests && ls vdev_*.py | sed 's/\.py$//' | grep -v -x -f <(cat cfw/targets/*/tests.txt 2>/dev/null | grep -v -e '^#' -e '^vdev_base$'))
fi

if [ -z "${NOBUILD:-}" ]; then
  echo "== Build $TARGET"
  CC=${CC:-clang} $PY -c "import sys; sys.path.insert(0,'cfw/tools'); import build; build.build('cfw/targets/$TARGET/target.json')" \
    | grep -E "^- Result|ERROR|wrote .*\.ppf" || true
  grep -q 'Result: \*\*PASS' $OUT/*.md || { echo "BUILD EN ÉCHEC (détails : $OUT/*.md)"; exit 1; }
fi

IMG=$(ls $OUT/*.ppf | head -1)
SHA=$( (sha256sum "$IMG" 2>/dev/null || shasum -a 256 "$IMG") | cut -c1-12)
export VPLAY_DIR=${VPLAY_DIR:-$HOME/.cache/vplay/$TARGET-$SHA}
mkdir -p "$VPLAY_DIR"
cp "$IMG" "$VPLAY_DIR/img.ppf"
cp "$OUT/link.map" "$VPLAY_DIR/"
echo "== Image $(basename "$IMG") ($SHA), dossier virtuel $VPLAY_DIR"

pass=(); fail=()
for t in $TESTS; do
  echo; echo "== $t"
  if $PY cfw/tests/$t.py "$VPLAY_DIR/img.ppf"; then pass+=("$t"); else fail+=("$t"); fi
done

echo; echo "== Résumé : ${#pass[@]} ok, ${#fail[@]} en échec   (captures : $VPLAY_DIR/shots)"
[ ${#fail[@]} -eq 0 ] || { echo "   en échec : ${fail[*]}"; exit 1; }
