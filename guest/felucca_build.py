#!/usr/bin/env python3
"""Construit un invité PGI de la famille Felucca (SLOOP, KARMA, ORBIT…) pour le Play+ (Avrek OS).

usage : guest/felucca_build.py NOM [sortie.pgi]     (NOM : sloop, …, voir FW ci-dessous)

Pour chaque firmware : son dépôt cloné dans ~/src/<dossier> à un commit fixé (jamais modifié ; ses en-têtes
générés par ses propres outils, dans son build/gen), notre fichier plateforme guest/<nom>/plat_<nom>.c (qui inclut
ses sources comme son harnais de test, puis guest/sdk/felucca_pgi.h), la matrice docs/correspondance/<nom>.json
(→ map_playplus.h par guest/loom/gen_map.py), clang pour le Cortex-M7, lié à 0x81E00000 (guest/loom/loom.ld).
Licence des firmwares : GPL-3.0 ; les .pgi sont pour un usage personnel, ne pas les distribuer."""
import os
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..')
SRC = os.path.expanduser('~/src')
SDRAM_BASE, SDRAM_LIMIT, MAGIC = 0x81E00000, 0x1FE000, 0x31494750
SDRAM8_BASE, SDRAM8_LIMIT = 0x81800000, 0x7FE000     # (grande zone, Avrek OS 0.6.0 : f['big'])


def gen_skiff(repo):
    gen = os.path.join(repo, 'build', 'gen')
    if not os.path.exists(os.path.join(gen, 'felucca_tables.h')):
        code = "import sys; sys.argv=['x']; sys.path.insert(0,'tools'); import build; build.generate()"
        r = subprocess.run([sys.executable, '-c', code], cwd=repo, capture_output=True, text=True)
        if r.returncode:
            raise SystemExit(r.stdout[-3000:] + r.stderr[-3000:])
    return [gen]


def gen_melodee(repo):
    gen = gen_skiff(repo)[0]
    if not os.path.exists(os.path.join(gen, 'x0x_drum_tables.h')):
        run([sys.executable, 'tools/gen_909_tables.py', os.path.join(gen, 'x0x_drum_tables.h')], cwd=repo)
    return [gen]


def gen_hortator(repo):
    gen = os.path.join(repo, 'build', 'sim', 'gen')
    if not os.path.exists(os.path.join(gen, 'sim_keymap.h')):
        os.makedirs(gen, exist_ok=True)
        code = ("import sys; sys.path.insert(0, 'tools'); import build; from pathlib import Path; "
                "build.GEN = Path(sys.argv[1]).resolve(); build.generate()")
        run([sys.executable, '-c', code, gen], cwd=repo)
        src = open(os.path.join(repo, 'firmware', 'hal', 'fm1_input.h')).read()
        i = src.index('static const int8_t FM1_KEYMAP')
        open(os.path.join(gen, 'sim_keymap.h'), 'w').write(src[i:src.index('};', i) + 2] + '\n')
    return [gen]


def gen_sloop_like(repo):
    """les en-têtes de tools/build.py generate() (police, icônes, tables, samples, kits, patchs FM6, logo)"""
    gen = os.path.join(repo, 'build', 'gen')
    if not os.path.exists(os.path.join(gen, 'felucca_tables.h')):
        code = "import sys; sys.argv=['x']; sys.path.insert(0,'tools'); import build; build.generate()"
        r = subprocess.run([sys.executable, '-c', code], cwd=repo, capture_output=True, text=True)
        if r.returncode:
            raise SystemExit(r.stdout + r.stderr)
    return [gen]


FW = {
    'sloop': dict(name='SLOOP', repo='sloop-fm1', url='https://github.com/isod89/sloop-fm1', commit='bac9c05',
                  gen=gen_sloop_like, inc=['firmware/src', 'firmware/hal']),
    'orbit': dict(name='ORBIT', repo='orbit', url='https://github.com/dspaudio/orbit', commit='1dbb77d',
                  gen=gen_sloop_like, inc=['firmware/src', 'firmware/hal', 'tests'], harness=True),
    'skiff': dict(name='SKIFF', repo='skiff', url='https://github.com/illuminus5/skiff', commit='392c1df',
                  gen=gen_skiff, inc=['firmware/src', 'firmware/hal', 'web/emu', 'tests'], harness=True,
                  defs=['-DFELUCCA_VERSION="v2.7.2"'], big=True),
    'melodee': dict(name='MELODEE', repo='melodee', url='https://github.com/keremimo/melodee', commit='165d177a',
                    gen=gen_melodee, inc=['firmware/src', 'firmware/hal', 'web/emu', 'tests'], harness=True, big=True),
    'coralflower': dict(name='CORALFLOWER', repo='CoralFlower', url='https://github.com/FicusFicusov/CoralFlower',
                        commit='46790b9', gen=gen_skiff,
                        inc=['tools/emu/web/compat', 'firmware/src', 'firmware/hal', 'tools/emu/web', 'tools/emu', 'tests'],
                        harness=True, big=True, std='gnu11', defs=['-DEMU_WEB', '-DFELUCCA_VERSION="CoralFlower"']),
    'hortator': dict(name='HORTATOR', repo='hortator', url='https://github.com/DeadActive/hortator', commit='44c9ad4',
                     gen=gen_hortator, inc=['firmware/src', 'tests'], harness=True, big=True,
                     defs=['-DFELUCCA_VERSION="DRUM-PLAY+"']),
    'karma': dict(name='KARMA', repo='karma-fm1', url='https://github.com/eatyourgreensstudio-dotcom/karma-fm1',
                  commit='1d67ab9', gen=gen_sloop_like, inc=['firmware/src', 'firmware/hal', 'tests'], harness=True,
                  cxx=dict(dir='firmware/mi', flags=['-std=c++11', '-DTEST', '-fno-rtti', '-fno-threadsafe-statics',
                                                     '-fno-use-cxa-atexit', '-nostdinc++'],
                           inc=['firmware/mi', 'firmware/cxx'])),
}


def run(cmd, **kw):
    r = subprocess.run(cmd, capture_output=True, text=True, **kw)
    if r.returncode:
        sys.stderr.write(r.stdout[-6000:] + r.stderr[-12000:])
        raise SystemExit(f'échec : {" ".join(cmd[:3])} ...')
    return r


def build(nom, out=None):
    f = FW[nom]
    repo = os.path.join(SRC, f['repo'])
    if not os.path.isdir(repo):
        run(['git', 'clone', '-q', '--recurse-submodules', f['url'], repo])
    run(['git', '-C', repo, 'checkout', '-q', f['commit']])
    gens = f['gen'](repo)
    bdir = os.path.join(HERE, nom, 'build')
    os.makedirs(bdir, exist_ok=True)
    out = out or os.path.join(HERE, nom, nom + '.pgi')
    run([sys.executable, os.path.join(HERE, 'loom', 'gen_map.py'), os.path.join(ROOT, 'docs', 'correspondance', nom + '.json'),
         os.path.join(bdir, 'map_playplus.h')])
    shim = os.path.join(HERE, 'sdk', 'hostshim') if f.get('harness') else os.path.join(HERE, 'loom', 'shim')
    inc = ['-isystem', shim, '-I', bdir, '-I', os.path.join(HERE, 'sdk'),
           '-I', os.path.join(ROOT, 'cfw', 'src')] + [a for g in gens for a in ('-I', g)] + \
          [a for d in f['inc'] for a in ('-I', os.path.join(repo, d))]
    cc = ['clang', '--target=thumbv7em-none-eabihf', '-mcpu=cortex-m7', '-mfpu=fpv5-d16', '-mfloat-abi=hard', '-mthumb',
          '-ffreestanding', '-fno-builtin', '-ffp-contract=off', '-std=' + f.get('std', 'gnu99'), '-ffunction-sections', '-fdata-sections',
          '-fno-common', '-fno-exceptions', '-fno-unwind-tables', '-fno-asynchronous-unwind-tables', '-w',
          '-DPGI_GUEST_BASE=' + ('PGI_SDRAM8_BASE' if f.get('big') else 'PGI_SDRAM_BASE'), '-O2'] + inc + f.get('defs', [])
    objs = []
    for s, o in [(os.path.join(HERE, nom, f'plat_{nom}.c'), f'plat_{nom}.o'),
                 (os.path.join(HERE, 'sdk', 'aeabi.c'), 'aeabi.o')] + \
            ([(os.path.join(HERE, 'sdk', 'hostlibc.c'), 'hostlibc.o')] if f.get('harness') else []) + \
            [(os.path.join(HERE, 'sdk', 'libm_lite.c'), 'libm_lite.o'), (os.path.join(HERE, 'sdk', 'rtconv.c'), 'rtconv.o')] + [ (os.path.join(HERE, 'vector37', 'rt64.c'), 'rt64.o')] + \
            [(os.path.join(repo, x), os.path.basename(x)[:-2] + '.o') for x in f.get('units', [])]:
        run(cc + ['-c', s, '-o', os.path.join(bdir, o)])
        objs.append(os.path.join(bdir, o))
    if f.get('cxx'):                            # (la partie C++ : un .o par .cc, comme le firmware)
        x = f['cxx']
        cxx = ['clang++'] + [a for a in cc[1:] if not a.startswith('-std=')] + x['flags'] + \
            [a for d in x['inc'] for a in ('-I', os.path.join(repo, d))]
        for dp, _, fs in sorted(os.walk(os.path.join(repo, x['dir']))):
            for fn in sorted(fs):
                if fn.endswith('.cc'):
                    o = os.path.join(bdir, 'cxx_' + os.path.relpath(os.path.join(dp, fn), repo).replace('/', '_')[:-3] + '.o')
                    if not os.path.exists(o) or os.path.getmtime(o) < os.path.getmtime(os.path.join(dp, fn)):
                        run(cxx + ['-c', os.path.join(dp, fn), '-o', o])
                    objs.append(o)
    if f.get('cxx'):
        o = os.path.join(bdir, 'cxxrt.o')
        run(cxx + ['-c', os.path.join(HERE, 'sdk', 'cxxrt.cc'), '-o', o])
        objs.append(o)
    elf = os.path.join(bdir, nom + '.elf')
    ld = os.path.join(HERE, 'sdk', 'sdram8.ld') if f.get('big') else os.path.join(HERE, 'loom', 'loom.ld')
    run(['ld.lld', '-T', ld, '--gc-sections', '-e', 'pgi_header', '-u', 'pgi_header',
         '--error-limit=0', '-Map', os.path.join(bdir, nom + '.map'), '-o', elf] + objs)
    run(['llvm-objcopy', '-O', 'binary', '--only-section=.text', '--only-section=.data', elf, out])
    img = open(out, 'rb').read()
    magic, abi, image_end, bss_end = struct.unpack_from('<4I', img, 0)
    base, = struct.unpack_from('<I', img, 48)
    if 0 < image_end - base - len(img) < 8:      # (.data d'une taille impaire : l'en-tête compte jusqu'à l'alignement)
        img += bytes(image_end - base - len(img))
        open(out, 'wb').write(img)
    zb, zl = (SDRAM8_BASE, SDRAM8_LIMIT) if f.get('big') else (SDRAM_BASE, SDRAM_LIMIT)
    if magic != MAGIC or base != zb or image_end != base + len(img) or bss_end > base + zl:
        raise SystemExit(f'image incohérente : magie {magic:#x} base {base:#x} fin {image_end:#x} bss {bss_end:#x}')
    print(f'{out} : {len(img)} o, mémoire totale {bss_end - base} o / {zl} (SDRAM {base:#x})')
    return out


if __name__ == '__main__':
    build(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else None)
