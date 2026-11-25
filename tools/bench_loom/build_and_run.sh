#!/usr/bin/env bash
# Banc de charge de LOOM-8 (charlesvestal/fm1-loom : MPL-2.0 / GPL-3.0) pour le Cortex-M7 du Play+.
# Clone LOOM à un commit fixé (non versionné ici), génère ses tables, compile son DSP pour Cortex-M7 avec
# clang, puis mesure dans Unicorn les instructions par bloc de 128 trames. Résultats : results_loom.json.
set -euo pipefail
cd "$(dirname "$0")"
HERE=$(pwd)
LOOM=${LOOM:-/tmp/fm1-loom}
if [ ! -d "$LOOM" ]; then
  git clone https://github.com/charlesvestal/fm1-loom "$LOOM"
  git -C "$LOOM" checkout 22d96133d512137f8fafed274de0ca10ce31a76a
  git -C "$LOOM" submodule update --init base
  git -C "$LOOM/base" checkout 7337375c15e9db26f23b0c68a1db61429add2f48
fi
mkdir -p "$LOOM/build/gen"
( cd "$LOOM" && python3 tools/gen_wavetables.py build/gen/wavetables.h && python3 tools/gen_samples.py build/gen/samples.h >/dev/null && python3 tools/gen_presets.py build/gen/presets.h )
mkdir -p build && cd build
FL="--target=thumbv7em-none-eabihf -mcpu=cortex-m7 -mfpu=fpv5-sp-d16 -mfloat-abi=hard -O2 -ffreestanding -fno-builtin -ffp-contract=off -std=gnu99 -ffunction-sections -fdata-sections -isystem $HERE/shim -I$LOOM/app/dsp -I$LOOM/base/include -I$LOOM/build/gen -Wno-everything"
for f in synth drumline d909 digisophie sample bass303 fx; do clang $FL -c "$LOOM/app/dsp/$f.c" -o "$f.o"; done
clang $FL -c ../bench_loom.c -o bench_loom.o
ld.lld -T ../link.ld --gc-sections -e bench_block -u bench_setup -u bench_peak -u bench_npresets -u bench_preset_engine *.o -o loom.elf
llvm-size -A loom.elf | grep -E "^\.(text|data|bss)"
"$HERE/../../.venv/bin/python" ../run_loom.py
