#!/usr/bin/env python3
"""Construit un invité PGI (voir cfw/src/pgi.h).

usage: guest/build.py sortie.pgi source.c [source2.c ...]

clang pour Cortex-M7 (FPU double précision, ABI « hard »), lié à 0x20200000 par guest/sdk/pgi.ld,
puis image brute (.text + .data). Vérifie l'en-tête et les limites."""
import os
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
SDK = os.path.join(HERE, 'sdk')
SRC = os.path.join(HERE, '..', 'cfw', 'src')
BASE, LIMIT, MAGIC = 0x20200000, 0x80000, 0x31494750
CFLAGS = ['--target=thumbv7em-none-eabihf', '-mcpu=cortex-m7', '-mfpu=fpv5-d16', '-mfloat-abi=hard',
          '-mthumb', '-O2', '-ffreestanding', '-fno-common', '-ffunction-sections', '-fdata-sections',
          '-fno-exceptions', '-fno-unwind-tables', '-fno-asynchronous-unwind-tables', '-Wall', '-Wextra',
          '-I', SDK, '-I', SRC]


def build(out, sources):
    tmp = tempfile.mkdtemp(prefix='pgi-')
    objs = []
    for s in list(sources) + [os.path.join(SDK, 'mini_libc.c')]:
        o = os.path.join(tmp, os.path.basename(s) + '.o')
        subprocess.run(['clang'] + CFLAGS + ['-c', s, '-o', o], check=True)
        objs.append(o)
    elf = os.path.join(tmp, 'guest.elf')
    subprocess.run(['ld.lld', '-T', os.path.join(SDK, 'pgi.ld'), '--gc-sections', '-e', 'pgi_header',
                    '-o', elf] + objs, check=True)
    subprocess.run(['llvm-objcopy', '-O', 'binary', '--only-section=.text', '--only-section=.data', elf, out],
                   check=True)
    img = open(out, 'rb').read()
    magic, abi, image_end, bss_end = struct.unpack_from('<4I', img, 0)
    if magic != MAGIC:
        raise SystemExit('en-tête PGI absent en tête de l’image (section .pgi_header)')
    base = struct.unpack_from('<I', img, 48)[0] if abi >= 2 else BASE
    if base != BASE:
        raise SystemExit(f'base {base:#x} : ce script lie en OCRAM (LOOM-8 et les gros invités : guest/loom)')
    if image_end != BASE + len(img):
        raise SystemExit(f'image_end {image_end:#x} != {BASE + len(img):#x}')
    if bss_end > BASE + LIMIT:
        raise SystemExit(f'trop gros : {bss_end - BASE} o > {LIMIT}')
    print(f'{out} : {len(img)} o, RAM totale {bss_end - BASE} o / {LIMIT}')
    return out


if __name__ == '__main__':
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    build(sys.argv[1], sys.argv[2:])
