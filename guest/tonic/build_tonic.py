#!/usr/bin/env python3
"""Construit FM1 Tonic (luigirossi-music/fm1-tonic) en invité PGI pour le Play+ : guest/tonic/tonic.pgi.

usage: guest/tonic/build_tonic.py [sortie.pgi]

1. clone luigirossi-music/fm1-tonic à un commit fixé dans $TONIC (défaut ~/src/fm1-tonic), non versionné ici,
   jamais modifié ;
2. génère ses en-têtes avec ses propres outils (police, tables de batterie, mots du lecteur d'écran, banque de
   préréglages presets/*.tnp et projet de départ presets/01_*.tnp), comme web/emu/build.sh ;
3. la matrice docs/correspondance/tonic.json (ou $TONIC_MAP) → map_playplus.h (guest/loom/gen_map.py) ;
4. compile pour le Cortex-M7 du Play+ avec clang : plat_tonic.c (plateforme + interface, -Os comme le FM-1),
   le DSP, le séquenceur et le moteur en -O2 (la liste de web/emu/build.sh), -ffp-contract=off comme Tonic ;
5. lie à 0x81E00000 (guest/loom/loom.ld) et vérifie l'en-tête.

Licence : Tonic GPL-3.0 (Luigi Rossi ; Felucca, Hügelton Instruments). Le .pgi est pour un usage personnel :
ne pas le distribuer."""
import glob
import os
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..', '..')
TONIC = os.environ.get('TONIC', os.path.expanduser('~/src/fm1-tonic'))
TONIC_COMMIT = 'cf0b9ec'                        # firmware 1.8g
O2 = ['dsp/tonic.c', 'dsp/po32/po32_synth.c', 'dsp/drum_tables.c', 'dsp/breaks.c', 'dsp/fxbus.c', 'dsp/master.c',
      'seq/sequencer.c', 'seq/pattern.c', 'seq/motion.c', 'app/engine.c']
SDRAM_BASE, SDRAM_LIMIT, MAGIC = 0x81E00000, 0x1FE000, 0x31494750
MAP = os.environ.get('TONIC_MAP', os.path.join(ROOT, 'docs', 'correspondance', 'tonic.json'))


def run(cmd, **kw):
    r = subprocess.run(cmd, capture_output=True, text=True, **kw)
    if r.returncode:
        sys.stderr.write(r.stdout + r.stderr)
        raise SystemExit(f'échec : {cmd[0]} ...')
    return r


def fetch(bdir):
    if not os.path.isdir(TONIC):
        run(['git', 'clone', '-q', 'https://github.com/luigirossi-music/fm1-tonic', TONIC])
    run(['git', '-C', TONIC, 'checkout', '-q', TONIC_COMMIT])
    gen = os.path.join(bdir, 'gen')
    os.makedirs(gen, exist_ok=True)
    py = sys.executable
    t = lambda *a: os.path.join(TONIC, *a)     # noqa: E731
    if not os.path.exists(os.path.join(gen, 'felucca_font.h')):
        run([py, t('tools', 'gen_font.py'), os.path.join(gen, 'felucca_font.h')], cwd=TONIC)
    if not os.path.exists(os.path.join(gen, 'x0x_drum_tables.h')):
        run([py, t('tools', 'gen_drum_tables.py'), os.path.join(gen, 'x0x_drum_tables.h')], cwd=TONIC)
    run([py, t('tools', 'gen_words.py'), t('firmware', 'words.tsv'), os.path.join(gen, 'words'),
         os.path.join(gen, 'ann_words.h')], cwd=TONIC)
    run([py, t('tools', 'gen_preset_bank.py'), os.path.join(gen, 'tonic_bank.h'), 'presets'], cwd=TONIC)
    start = sorted(glob.glob(t('presets', '01_*.tnp')))[:1]
    run([py, t('tools', 'gen_preset_bank.py'), '--start', os.path.join(gen, 'tonic_start.h')] + start, cwd=TONIC)
    return gen


def build(out):
    bdir = os.path.join(HERE, 'build')
    os.makedirs(bdir, exist_ok=True)
    gen = fetch(bdir)
    run([sys.executable, os.path.join(ROOT, 'guest', 'loom', 'gen_map.py'), MAP, os.path.join(bdir, 'map_playplus.h')])
    src = os.path.join(TONIC, 'firmware', 'src')
    inc = ['-isystem', os.path.join(ROOT, 'guest', 'loom', 'shim'), f'-I{src}', f'-I{os.path.join(src, "dsp")}',
           f'-I{gen}', f'-I{bdir}', '-I', os.path.join(ROOT, 'guest', 'sdk'), '-I', os.path.join(ROOT, 'cfw', 'src')]
    common = ['clang', '--target=thumbv7em-none-eabihf', '-mcpu=cortex-m7', '-mfpu=fpv5-d16', '-mfloat-abi=hard',
              '-mthumb', '-ffreestanding', '-fno-builtin', '-ffp-contract=off', '-std=gnu99', '-ffunction-sections',
              '-fdata-sections', '-fno-common', '-fno-exceptions', '-fno-unwind-tables',
              '-fno-asynchronous-unwind-tables', '-w', '-DPGI_GUEST_BASE=PGI_SDRAM_BASE',
              '-DX0X_VERSION="PLAY+"'] + inc
    objs = []

    def cc(s, obj, opt, extra=()):
        run(common + [opt, *extra, '-c', s, '-o', obj])
        objs.append(obj)
    cc(os.path.join(HERE, 'plat_tonic.c'), os.path.join(bdir, 'plat_tonic.o'), '-Os')
    for f in O2:
        s = os.path.join(src, f)
        cc(s, os.path.join(bdir, f.replace('/', '_')[:-2] + '.o'), '-O2', [f'-I{os.path.dirname(s)}'])
    cc(os.path.join(ROOT, 'guest', 'loom', 'rt.c'), os.path.join(bdir, 'rt.o'), '-O2')
    cc(os.path.join(ROOT, 'guest', 'sdk', 'mini_libc.c'), os.path.join(bdir, 'mini_libc.o'), '-O2')
    cc(os.path.join(ROOT, 'guest', 'vector37', 'rt64.c'), os.path.join(bdir, 'rt64.o'), '-O2')
    elf = os.path.join(bdir, 'tonic.elf')
    run(['ld.lld', '-T', os.path.join(ROOT, 'guest', 'loom', 'loom.ld'), '--gc-sections', '-e', 'pgi_header',
         '-u', 'pgi_header', '--error-limit=0', '-Map', os.path.join(bdir, 'tonic.map'), '-o', elf] + objs)
    run(['llvm-objcopy', '-O', 'binary', '--only-section=.text', '--only-section=.data', elf, out])
    img = open(out, 'rb').read()
    magic, abi, image_end, bss_end = struct.unpack_from('<4I', img, 0)
    base, = struct.unpack_from('<I', img, 48)
    if magic != MAGIC or base != SDRAM_BASE or image_end != base + len(img) or bss_end > base + SDRAM_LIMIT:
        raise SystemExit(f'image incohérente : magie {magic:#x} base {base:#x} fin {image_end:#x} bss {bss_end:#x}')
    print(f'{out} : {len(img)} o, mémoire totale {bss_end - base} o / {SDRAM_LIMIT} (SDRAM {base:#x})')
    return out


if __name__ == '__main__':
    build(sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, 'tonic.pgi'))
