#!/usr/bin/env bash
# Banc de mesure CPU de Braids (Mutable Instruments, licence MIT) pour le Cortex-M7 du Play+.
# Clone le code de Braids (non versionné ici), compile pour Cortex-M7 avec clang, puis mesure dans
# Unicorn les instructions par échantillon de chaque modèle. Résultats : results.json.
set -euo pipefail
cd "$(dirname "$0")"
EURO=${EURO:-/tmp/eurorack}
if [ ! -d "$EURO" ]; then
  git clone https://github.com/pichenettes/eurorack "$EURO"
  git -C "$EURO" checkout f4cfa7697095a28d292bff758beb51deb49b5a86
  git -C "$EURO" submodule update --init stmlib
  git -C "$EURO/stmlib" checkout e3bd7c9cc00e4364166f9905c0509b6ffd0535ec
fi
mkdir -p build && cd build
FLAGS="--target=thumbv7em-none-eabihf -mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard -O2 -ffreestanding -nostdinc++ -fno-exceptions -fno-rtti -fno-threadsafe-statics -ffunction-sections -fdata-sections -fno-builtin -DTEST -isystem ../shim -I$EURO -std=c++11 -Wno-everything"
for f in ../bench.cc $EURO/braids/macro_oscillator.cc $EURO/braids/analog_oscillator.cc $EURO/braids/digital_oscillator.cc $EURO/braids/resources.cc $EURO/stmlib/utils/random.cc; do
  clang++ $FLAGS -c "$f" -o "$(basename "$f" .cc).o"
done
ld.lld -T ../link.ld --gc-sections -e bench_render -u bench_init -u bench_shape -u bench_peak *.o -o bench.elf
llvm-size -A bench.elf | grep -E "^\.(text|data|bss)"
../../../.venv/bin/python ../run.py
