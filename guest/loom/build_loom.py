#!/usr/bin/env python3
"""Construit LOOM-8 en invité PGI pour le Play+ (mode invité) : guest/loom/loom.pgi par défaut.

usage: guest/loom/build_loom.py [sortie.pgi]

1. clone charlesvestal/fm1-loom (et son sous-module fm1-base) à des commits fixés dans $LOOM
   (défaut ~/src/fm1-loom), non versionnés ici ;
2. génère ses en-têtes (polices, tables d'ondes, préréglages, ressources) avec son propre outil
   (base/tools/build.py app --host : il construit aussi le simulateur PC de LOOM-8, avec gcc) ;
3. compile pour le Cortex-M7 du Play+ avec clang : l'interface en -Oz (comme LOOM-8 sur le FM-1), le DSP
   et le séquenceur en -O2 (sa liste « o2 » de fm1.toml), notre plateforme plat_playplus.c ;
4. lie à 0x81E00000 (loom.ld) et fait l'image brute, vérifiée comme les autres invités (guest/build.py).

Licences : LOOM-8 et fm1-base (MPL-2.0 par fichier, GPL-3.0 pour le tout). Le .pgi est pour un usage
personnel : ne pas le distribuer."""
import os
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..', '..')
LOOM = os.environ.get('LOOM', os.path.expanduser('~/src/fm1-loom'))
LOOM_COMMIT = '22d96133d512137f8fafed274de0ca10ce31a76a'
BASE_COMMIT = '7337375c15e9db26f23b0c68a1db61429add2f48'
O2 = ['dsp/synth.c', 'dsp/drumline.c', 'dsp/digisophie.c', 'dsp/sample.c', 'dsp/fx.c', 'dsp/bass303.c',
      'dsp/d909.c', 'dsp/bench.c', 'seq/pattern.c', 'seq/seq.c']
ENTRY = ['app_init', 'app_render', 'app_frame', 'app_ui_init', 'app_input_only', 'pgi_header']
SDRAM_BASE, SDRAM_LIMIT, MAGIC = 0x81E00000, 0x1FE000, 0x31494750


def run(cmd, **kw):
    r = subprocess.run(cmd, capture_output=True, text=True, **kw)
    if r.returncode:
        sys.stderr.write(r.stdout + r.stderr)
        raise SystemExit(f'échec : {cmd[0]} ...')
    return r


def fetch():
    if not os.path.isdir(LOOM):
        run(['git', 'clone', '-q', 'https://github.com/charlesvestal/fm1-loom', LOOM])
    run(['git', '-C', LOOM, 'checkout', '-q', LOOM_COMMIT])
    run(['git', '-C', LOOM, 'submodule', 'update', '--init', '-q', 'base'])
    run(['git', '-C', os.path.join(LOOM, 'base'), 'checkout', '-q', BASE_COMMIT])
    gen = os.path.join(LOOM, 'app', 'build', 'gen')
    if not os.path.exists(os.path.join(gen, 'res.bin')):
        run([sys.executable, 'base/tools/build.py', 'app', '--host'], cwd=LOOM)
    return gen


MAP = os.environ.get('LOOM_MAP', os.path.join(ROOT, 'docs', 'correspondance', 'loom-8.json'))


def build(out):
    gen = fetch()
    bdir = os.path.join(HERE, 'build')
    os.makedirs(bdir, exist_ok=True)
    # la matrice de la page de câblage → tables C de plat_playplus.c (map_playplus.h)
    run([sys.executable, os.path.join(HERE, 'gen_map.py'), MAP, os.path.join(bdir, 'map_playplus.h')])
    inc = ['-isystem', os.path.join(HERE, 'shim')] + [f'-I{os.path.join(LOOM, d)}' for d in
                                                       ('base/device', 'base/include', 'base/lib', 'app')] + \
          [f'-I{gen}', f'-I{bdir}', '-I', os.path.join(ROOT, 'guest', 'sdk'), '-I', os.path.join(ROOT, 'cfw', 'src')]
    common = ['clang', '--target=thumbv7em-none-eabihf', '-mcpu=cortex-m7', '-mfpu=fpv5-d16', '-mfloat-abi=hard',
              '-mthumb', '-ffreestanding', '-fno-builtin', '-ffp-contract=off', '-std=gnu99', '-ffunction-sections',
              '-fdata-sections', '-fno-common', '-fno-exceptions', '-fno-unwind-tables',
              '-fno-asynchronous-unwind-tables', '-w', '-DPGI_GUEST_BASE=PGI_SDRAM_BASE'] + inc
    objs = []
    # Accélération des encodeurs de LOOM (ui.c, accel : x2 à x6 si les crans arrivent à moins de 60 ms). Les
    # potards du Play+ donnent 64 crans par tour (pilote 0x600af43c, décalage 6 sur 4096 positions) et les
    # envoient vite : avec l'accélération, un geste rapide sautait de 6 en 6 (constaté le 26/11). Copies
    # app_pp.c / ui_pp.c dans bdir, sans accélération par défaut ; LOOM_ACCEL=1 la garde.
    app = open(os.path.join(LOOM, 'app', 'app.c')).read()
    ui = open(os.path.join(LOOM, 'app', 'ui.c')).read()
    if os.environ.get('LOOM_ACCEL', '0') != '1':
        if '#define ACCEL_OFF accel_off\n' not in ui:
            raise SystemExit('ui.c de LOOM : ACCEL_OFF introuvable (version changée ?)')
        ui = ui.replace('#define ACCEL_OFF accel_off\n', '#define ACCEL_OFF 1   /* Play+ : pas d\'accélération */\n')
    if '#include "ui.c"' not in app:
        raise SystemExit('app.c de LOOM : #include "ui.c" introuvable')
    open(os.path.join(bdir, 'ui_pp.c'), 'w').write(ui)
    app = app.replace('#include "ui.c"', '#include "ui_pp.c"')
    for name in sorted(os.listdir(os.path.join(LOOM, 'app'))):    # app.c, déplacé dans bdir, doit garder ses
        if name.endswith(('.c', '.h')) and f'#include "{name}"' in app:   # voisins de app/ (res.c existe aussi
            app = app.replace(f'#include "{name}"',                      # dans base/device)
                              '#include "%s"' % os.path.join(LOOM, 'app', name))
    open(os.path.join(bdir, 'app_pp.c'), 'w').write(app)

    def cc(src, obj, opt, extra=()):
        run(common + [opt, *extra, '-c', src, '-o', obj])
        objs.append(obj)
    cc(os.path.join(HERE, 'plat_playplus.c'), os.path.join(bdir, 'plat_playplus.o'), '-Oz')
    for f in O2:
        src = os.path.join(LOOM, 'app', f)
        cc(src, os.path.join(bdir, f.replace('/', '_')[:-2] + '.o'), '-O2', [f'-I{os.path.dirname(src)}'])
    cc(os.path.join(HERE, 'rt.c'), os.path.join(bdir, 'rt.o'), '-O2')
    cc(os.path.join(ROOT, 'guest', 'sdk', 'mini_libc.c'), os.path.join(bdir, 'mini_libc.o'), '-O2')
    cc(os.path.join(HERE, 'res_blob.S'), os.path.join(bdir, 'res_blob.o'), '-O2',
       [f'-DLOOM_RES_BIN="{os.path.join(gen, "res.bin")}"'])
    elf = os.path.join(bdir, 'loom.elf')
    run(['ld.lld', '-T', os.path.join(HERE, 'loom.ld'), '--gc-sections', '-e', 'pgi_header', '--error-limit=0',
         *[a for e in ENTRY for a in ('-u', e)], '-Map', os.path.join(bdir, 'loom.map'), '-o', elf] + objs)
    run(['llvm-objcopy', '-O', 'binary', '--only-section=.text', '--only-section=.data', elf, out])
    img = open(out, 'rb').read()
    magic, abi, image_end, bss_end = struct.unpack_from('<4I', img, 0)
    base, = struct.unpack_from('<I', img, 48)
    if magic != MAGIC or base != SDRAM_BASE or image_end != base + len(img) or bss_end > base + SDRAM_LIMIT:
        raise SystemExit(f'image incohérente : magie {magic:#x} base {base:#x} fin {image_end:#x} bss {bss_end:#x}')
    print(f'{out} : {len(img)} o, mémoire totale {bss_end - base} o / {SDRAM_LIMIT} (SDRAM {base:#x})')
    return out


if __name__ == '__main__':
    build(sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, 'loom.pgi'))
