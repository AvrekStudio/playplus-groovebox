#!/usr/bin/env python3
"""Construit le banc d'essai du Play+ (guest/banc/banc.c) : un invité PGI dans la grande zone SDRAM (8 Mo).

usage: guest/banc/build_banc.py [sortie.pgi]      (défaut : guest/banc/banc.pgi)

Génère au passage, dans guest/banc/build/ :
  * banc_font.h : une police 5 x 7 à nous (dessinée ici, ASCII 32..95 ; minuscules affichées en majuscules) ;
  * kernel.S   : un noyau de 16 384 instructions 32 bits sans branchement (64 Ko, plus que le cache d'instructions de
                 32 Ko), indépendant de sa position, pour comparer l'exécution du code depuis la SDRAM et l'OCRAM."""
import os
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
SDK = os.path.join(ROOT, 'guest', 'sdk')
BASE, LIMIT = 0x81800000, 0x7FE000

# 5 x 7, une chaîne de 7 rangées de 5 colonnes par caractère ('#' allumé)
FONT = {
    ' ': ['.....'] * 7,
    '!': ['..#..', '..#..', '..#..', '..#..', '..#..', '.....', '..#..'],
    '"': ['.#.#.', '.#.#.', '.....', '.....', '.....', '.....', '.....'],
    '#': ['.#.#.', '#####', '.#.#.', '.#.#.', '#####', '.#.#.', '.....'],
    '%': ['##...', '##..#', '...#.', '..#..', '.#...', '#..##', '...##'],
    "'": ['..#..', '..#..', '.....', '.....', '.....', '.....', '.....'],
    '(': ['...#.', '..#..', '.#...', '.#...', '.#...', '..#..', '...#.'],
    ')': ['.#...', '..#..', '...#.', '...#.', '...#.', '..#..', '.#...'],
    '*': ['.....', '#.#.#', '.###.', '#####', '.###.', '#.#.#', '.....'],
    '+': ['.....', '..#..', '..#..', '#####', '..#..', '..#..', '.....'],
    ',': ['.....', '.....', '.....', '.....', '.##..', '..#..', '.#...'],
    '-': ['.....', '.....', '.....', '#####', '.....', '.....', '.....'],
    '.': ['.....', '.....', '.....', '.....', '.....', '.##..', '.##..'],
    '/': ['.....', '....#', '...#.', '..#..', '.#...', '#....', '.....'],
    '0': ['.###.', '#...#', '#..##', '#.#.#', '##..#', '#...#', '.###.'],
    '1': ['..#..', '.##..', '..#..', '..#..', '..#..', '..#..', '.###.'],
    '2': ['.###.', '#...#', '....#', '...#.', '..#..', '.#...', '#####'],
    '3': ['#####', '...#.', '..#..', '...#.', '....#', '#...#', '.###.'],
    '4': ['...#.', '..##.', '.#.#.', '#..#.', '#####', '...#.', '...#.'],
    '5': ['#####', '#....', '####.', '....#', '....#', '#...#', '.###.'],
    '6': ['..##.', '.#...', '#....', '####.', '#...#', '#...#', '.###.'],
    '7': ['#####', '....#', '...#.', '..#..', '.#...', '.#...', '.#...'],
    '8': ['.###.', '#...#', '#...#', '.###.', '#...#', '#...#', '.###.'],
    '9': ['.###.', '#...#', '#...#', '.####', '....#', '...#.', '.##..'],
    ':': ['.....', '.##..', '.##..', '.....', '.##..', '.##..', '.....'],
    ';': ['.....', '.##..', '.##..', '.....', '.##..', '..#..', '.#...'],
    '<': ['...#.', '..#..', '.#...', '#....', '.#...', '..#..', '...#.'],
    '=': ['.....', '.....', '#####', '.....', '#####', '.....', '.....'],
    '>': ['.#...', '..#..', '...#.', '....#', '...#.', '..#..', '.#...'],
    '?': ['.###.', '#...#', '....#', '...#.', '..#..', '.....', '..#..'],
    '@': ['.###.', '#...#', '....#', '.##.#', '#.#.#', '#.#.#', '.###.'],
    'A': ['.###.', '#...#', '#...#', '#####', '#...#', '#...#', '#...#'],
    'B': ['####.', '#...#', '#...#', '####.', '#...#', '#...#', '####.'],
    'C': ['.###.', '#...#', '#....', '#....', '#....', '#...#', '.###.'],
    'D': ['###..', '#..#.', '#...#', '#...#', '#...#', '#..#.', '###..'],
    'E': ['#####', '#....', '#....', '####.', '#....', '#....', '#####'],
    'F': ['#####', '#....', '#....', '####.', '#....', '#....', '#....'],
    'G': ['.###.', '#...#', '#....', '#.###', '#...#', '#...#', '.####'],
    'H': ['#...#', '#...#', '#...#', '#####', '#...#', '#...#', '#...#'],
    'I': ['.###.', '..#..', '..#..', '..#..', '..#..', '..#..', '.###.'],
    'J': ['..###', '...#.', '...#.', '...#.', '...#.', '#..#.', '.##..'],
    'K': ['#...#', '#..#.', '#.#..', '##...', '#.#..', '#..#.', '#...#'],
    'L': ['#....', '#....', '#....', '#....', '#....', '#....', '#####'],
    'M': ['#...#', '##.##', '#.#.#', '#.#.#', '#...#', '#...#', '#...#'],
    'N': ['#...#', '#...#', '##..#', '#.#.#', '#..##', '#...#', '#...#'],
    'O': ['.###.', '#...#', '#...#', '#...#', '#...#', '#...#', '.###.'],
    'P': ['####.', '#...#', '#...#', '####.', '#....', '#....', '#....'],
    'Q': ['.###.', '#...#', '#...#', '#...#', '#.#.#', '#..#.', '.##.#'],
    'R': ['####.', '#...#', '#...#', '####.', '#.#..', '#..#.', '#...#'],
    'S': ['.####', '#....', '#....', '.###.', '....#', '....#', '####.'],
    'T': ['#####', '..#..', '..#..', '..#..', '..#..', '..#..', '..#..'],
    'U': ['#...#', '#...#', '#...#', '#...#', '#...#', '#...#', '.###.'],
    'V': ['#...#', '#...#', '#...#', '#...#', '#...#', '.#.#.', '..#..'],
    'W': ['#...#', '#...#', '#...#', '#.#.#', '#.#.#', '#.#.#', '.#.#.'],
    'X': ['#...#', '#...#', '.#.#.', '..#..', '.#.#.', '#...#', '#...#'],
    'Y': ['#...#', '#...#', '.#.#.', '..#..', '..#..', '..#..', '..#..'],
    'Z': ['#####', '....#', '...#.', '..#..', '.#...', '#....', '#####'],
    '[': ['.###.', '.#...', '.#...', '.#...', '.#...', '.#...', '.###.'],
    '\\': ['.....', '#....', '.#...', '..#..', '...#.', '....#', '.....'],
    ']': ['.###.', '...#.', '...#.', '...#.', '...#.', '...#.', '.###.'],
    '^': ['..#..', '.#.#.', '#...#', '.....', '.....', '.....', '.....'],
    '_': ['.....', '.....', '.....', '.....', '.....', '.....', '#####'],
}


def gen_font(path):
    rows = []
    for c in range(32, 96):
        g = FONT.get(chr(c), FONT['?'])
        assert len(g) == 7 and all(len(r) == 5 for r in g), chr(c)
        rows.append('{' + ', '.join('0x%02x' % sum(1 << (4 - i) for i in range(5) if r[i] == '#') for r in g) + '}')
    with open(path, 'w') as f:
        f.write('/* police 5 x 7 du banc (générée par build_banc.py) : ASCII 32..95, bit 4 = colonne de gauche */\n')
        f.write('static const unsigned char FONT57[64][7] = {\n    ' + ',\n    '.join(rows) + '};\n')


def gen_kernel(path, n=16384):
    with open(path, 'w') as f:
        f.write('@ noyau de %d instructions 32 bits sans branchement (build_banc.py) : r0 += r1, rendu dans r0\n' % n)
        f.write('    .syntax unified\n    .thumb\n    .section .text.banc_kernel,"ax",%progbits\n')
        f.write('    .global banc_kernel\n    .global banc_kernel_end\n    .type banc_kernel, %function\n    .align 4\n')
        f.write('banc_kernel:\n')
        for _ in range(n):
            f.write('    add.w r0, r0, r1\n')
        f.write('    bx lr\nbanc_kernel_end:\n')


def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode:
        sys.stderr.write(r.stdout + r.stderr)
        raise SystemExit('échec : ' + ' '.join(cmd[:3]))


def build(out=None):
    out = out or os.path.join(HERE, 'banc.pgi')
    bdir = os.path.join(HERE, 'build')
    os.makedirs(bdir, exist_ok=True)
    gen_font(os.path.join(bdir, 'banc_font.h'))
    gen_kernel(os.path.join(bdir, 'kernel.S'))
    cc = ['clang', '--target=thumbv7em-none-eabihf', '-mcpu=cortex-m7', '-mfpu=fpv5-d16', '-mfloat-abi=hard', '-mthumb',
          '-O2', '-ffreestanding', '-fno-builtin', '-fno-common', '-ffunction-sections', '-fdata-sections',
          '-fno-exceptions', '-fno-unwind-tables', '-fno-asynchronous-unwind-tables', '-Wall', '-Wextra',
          '-DPGI_GUEST_BASE=PGI_SDRAM8_BASE', '-I', bdir, '-I', SDK, '-I', os.path.join(ROOT, 'cfw', 'src')]
    objs = []
    for s in (os.path.join(HERE, 'banc.c'), os.path.join(bdir, 'kernel.S'), os.path.join(SDK, 'mini_libc.c'),
              os.path.join(ROOT, 'guest', 'vector37', 'rt64.c')):   # (divisions 64 bits)
        o = os.path.join(bdir, os.path.basename(s) + '.o')
        run(cc + ['-c', s, '-o', o])
        objs.append(o)
    elf = os.path.join(bdir, 'banc.elf')
    run(['ld.lld', '-T', os.path.join(SDK, 'sdram8.ld'), '--gc-sections', '-e', 'pgi_header', '-u', 'pgi_header',
         '-Map', os.path.join(bdir, 'banc.map'), '-o', elf] + objs)
    run(['llvm-objcopy', '-O', 'binary', '--only-section=.text', '--only-section=.data', elf, out])
    img = open(out, 'rb').read()
    magic, abi, image_end, bss_end = struct.unpack_from('<4I', img, 0)
    if 0 < image_end - BASE - len(img) < 8:      # (l'en-tête compte jusqu'à l'alignement)
        img += b'\0' * (image_end - BASE - len(img))
        open(out, 'wb').write(img)
    assert magic == 0x31494750 and image_end == BASE + len(img) and bss_end <= BASE + LIMIT, (hex(image_end), len(img))
    print('%s : %d o, mémoire totale %d o / %d (SDRAM %#x, ABI %d)' % (out, len(img), bss_end - BASE, LIMIT, BASE, abi))
    return out


if __name__ == '__main__':
    build(sys.argv[1] if len(sys.argv) > 1 else None)
