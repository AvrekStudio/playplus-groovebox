#!/usr/bin/env python3
"""Construit Vector37 (genkigenki/Vector37) en invité PGI pour le Play+ : guest/vector37/vector37.pgi.

usage: guest/vector37/build_vector37.py [sortie.pgi]

1. clone genkigenki/Vector37 à un commit fixé dans $VECTOR37 (défaut ~/src/Vector37), non versionné ici,
   jamais modifié ;
2. génère ses tables et sa police avec ses propres outils (tools/gen_tables.py, tools/gen_font.py) ;
3. la matrice docs/correspondance/vector37.json (ou $V37_MAP) → map_vector37.h (gen_map_v37.py) ;
4. compile plat_vector37.c (qui inclut tests/panel.h de Vector37, donc tout son firmware : moteur, interface,
   écran) pour le Cortex-M7 du Play+ avec clang, -O2 ;
5. lie à 0x81E00000 (le même script que LOOM-8) et vérifie l'en-tête.

Licence : Vector37 GPL-3.0. Le .pgi est pour un usage personnel : ne pas le distribuer."""
import os
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..', '..')
V37 = os.environ.get('VECTOR37', os.path.expanduser('~/src/Vector37'))
V37_COMMIT = '08742a4'                          # Vector37 0.3.2
SDRAM_BASE, SDRAM_LIMIT, MAGIC = 0x81E00000, 0x1FE000, 0x31494750
MAP = os.environ.get('V37_MAP', os.path.join(ROOT, 'docs', 'correspondance', 'vector37.json'))


def run(cmd, **kw):
    r = subprocess.run(cmd, capture_output=True, text=True, **kw)
    if r.returncode:
        sys.stderr.write(r.stdout + r.stderr)
        raise SystemExit(f'échec : {cmd[0]} ...')
    return r


def fetch(bdir):
    if not os.path.isdir(V37):
        run(['git', 'clone', '-q', 'https://github.com/genkigenki/Vector37', V37])
    run(['git', '-C', V37, 'checkout', '-q', V37_COMMIT])
    gen = os.path.join(bdir, 'gen')
    os.makedirs(gen, exist_ok=True)
    if not os.path.exists(os.path.join(gen, 'mk_tables.h')):
        run([sys.executable, os.path.join(V37, 'tools', 'gen_tables.py'), os.path.join(gen, 'mk_tables.h')], cwd=V37)
    if not os.path.exists(os.path.join(gen, 'felucca_font.h')):
        run([sys.executable, os.path.join(V37, 'tools', 'gen_font.py'), os.path.join(gen, 'felucca_font.h')], cwd=V37)
    return gen


def build(out):
    bdir = os.path.join(HERE, 'build')
    os.makedirs(bdir, exist_ok=True)
    gen = fetch(bdir)
    run([sys.executable, os.path.join(HERE, 'gen_map_v37.py'), MAP, os.path.join(bdir, 'map_vector37.h')])
    inc = ['-isystem', os.path.join(ROOT, 'guest', 'loom', 'shim'), f'-I{os.path.join(V37, "tests")}', f'-I{gen}',
           f'-I{bdir}', '-I', os.path.join(ROOT, 'guest', 'sdk'), '-I', os.path.join(ROOT, 'cfw', 'src')]
    common = ['clang', '--target=thumbv7em-none-eabihf', '-mcpu=cortex-m7', '-mfpu=fpv5-d16', '-mfloat-abi=hard',
              '-mthumb', '-ffreestanding', '-fno-builtin', '-std=gnu11', '-ffunction-sections', '-fdata-sections',
              '-fno-common', '-fno-exceptions', '-fno-unwind-tables', '-fno-asynchronous-unwind-tables', '-w',
              '-DPGI_GUEST_BASE=PGI_SDRAM_BASE', '-DMK_VERSION="playplus"'] + inc
    objs = []

    def cc(src, obj, opt):
        run(common + [opt, '-c', src, '-o', obj])
        objs.append(obj)
    cc(os.path.join(HERE, 'plat_vector37.c'), os.path.join(bdir, 'plat_vector37.o'), '-O2')
    cc(os.path.join(ROOT, 'guest', 'loom', 'rt.c'), os.path.join(bdir, 'rt.o'), '-O2')
    cc(os.path.join(ROOT, 'guest', 'sdk', 'mini_libc.c'), os.path.join(bdir, 'mini_libc.o'), '-O2')
    cc(os.path.join(HERE, 'rt64.c'), os.path.join(bdir, 'rt64.o'), '-O2')
    elf = os.path.join(bdir, 'vector37.elf')
    run(['ld.lld', '-T', os.path.join(ROOT, 'guest', 'loom', 'loom.ld'), '--gc-sections', '-e', 'pgi_header',
         '-u', 'pgi_header', '-Map', os.path.join(bdir, 'vector37.map'), '-o', elf] + objs)
    run(['llvm-objcopy', '-O', 'binary', '--only-section=.text', '--only-section=.data', elf, out])
    img = open(out, 'rb').read()
    magic, abi, image_end, bss_end = struct.unpack_from('<4I', img, 0)
    base, = struct.unpack_from('<I', img, 48)
    if magic != MAGIC or base != SDRAM_BASE or image_end != base + len(img) or bss_end > base + SDRAM_LIMIT:
        raise SystemExit(f'image incohérente : magie {magic:#x} base {base:#x} fin {image_end:#x} bss {bss_end:#x}')
    print(f'{out} : {len(img)} o, mémoire totale {bss_end - base} o / {SDRAM_LIMIT} (SDRAM {base:#x})')
    return out


if __name__ == '__main__':
    build(sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, 'vector37.pgi'))
