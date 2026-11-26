#!/usr/bin/env python3
"""Matrice SMK-37 (Vector37) → Play+ (JSON fm1-playplus-map/1, page de câblage) → map_vector37.h.

usage: gen_map_v37.py docs/correspondance/vector37.json build/map_vector37.h

Contrôles du SMK-37 (champ « fm1 » des liens, identifiants de Vector37 / tests/panel.h) :
  BTN0..BTN17 (boutons, numéros du HAL), ENC0..ENC7 (K1..K8), PAD0..PAD15 (pads : 0..7 rangée du haut,
  8..15 du bas), KEY0..KEY36 (touches, do à do), FADER0..FADER3, WHEEL_PITCH, WHEEL_MOD.
Côté Play+ (« playplus ») : pad:N, btn:N, knob:N, enc. « shift »: true = couche Shift du Play+ (B10).
Un potard ou l'encodeur vers un bouton, un pad ou une touche : « action » cran+ / cran− (un appui par cran) ;
vers K1..K8 : les crans ; vers un fader ou une roue : une valeur absolue que les crans montent et descendent."""
import json
import re
import sys

VT = ['VT_NONE', 'VT_BTN', 'VT_KEY', 'VT_PAD', 'VT_ENC', 'VT_FADER', 'VT_WHEEL']


def target(fm1):
    m = re.fullmatch(r'(BTN|KEY|PAD|ENC|FADER)(\d+)', fm1)
    if m:
        kind, n = m.group(1), int(m.group(2))
        lim = {'BTN': 18, 'KEY': 37, 'PAD': 16, 'ENC': 8, 'FADER': 4}[kind]
        if n >= lim:
            raise SystemExit(f'{fm1} : hors limites')
        return 'VT_' + kind, n
    if fm1 in ('WHEEL_PITCH', 'WHEEL_MOD'):
        return 'VT_WHEEL', 0 if fm1 == 'WHEEL_PITCH' else 1
    raise SystemExit(f'contrôle SMK-37 inconnu : {fm1}')


def main():
    src, out = sys.argv[1], sys.argv[2]
    d = json.load(open(src, encoding='utf-8'))
    pad = [[None] * 160 for _ in range(2)]
    btn = [[None] * 12 for _ in range(2)]
    knob = [[None] * 30 for _ in range(2)]
    enc = [[None] * 2 for _ in range(2)]
    for l in d['links']:
        t, n = target(l['fm1'])
        layer = 1 if l.get('shift') else 0
        pp, act = l['playplus'], l.get('action')
        sub = 1 if act == 'cran-' else 0
        if pp.startswith('pad:'):
            tab, i = pad, int(pp[4:])
        elif pp.startswith('btn:'):
            tab, i = btn, int(pp[4:])
            if i == 10:
                raise SystemExit('B10 (Shift du Play+) ne peut pas être une source')
        elif pp.startswith('knob:'):
            tab, i = knob, int(pp[5:]) * 2 + sub
        elif pp == 'enc':
            tab, i = enc, sub
        else:
            raise SystemExit(f'source Play+ inconnue : {pp}')
        if tab[layer][i] is not None:
            raise SystemExit(f'{pp} (couche {layer}) relié deux fois')
        # un bouton ou un pad du Play+ vers K1..K8, un fader ou une roue : un cran par appui (action)
        tact = 0 if tab in (knob, enc) else 1 if act == 'cran+' else 2 if act == 'cran-' else 0
        tab[layer][i] = (t, n, tact)

    def table(name, tab, n):
        rows = []
        for layer in tab:
            rows.append('{' + ', '.join('{%s, %d, %d}' % (e if e else ('VT_NONE', 0, 0)) for e in layer) + '}')
        return f'static const struct v37_map {name}[2][{n}] = {{\n  ' + ',\n  '.join(rows) + '\n};\n'

    h = ['/* Généré par guest/vector37/gen_map_v37.py depuis %s : ne pas modifier. */' % src.split('/')[-1],
         '#include <stdint.h>',
         'enum { %s };' % ', '.join(VT),
         'enum { W_PITCH, W_MOD };',
         'struct v37_map { uint8_t t, id, act; };   /* act : bouton / pad du Play+ vers un contrôle tournant : 1 cran +, 2 cran − */',
         '#define V37_BLACK 0x54Au   /* do# ré# fa# sol# la# dans l\'octave (touche 0 = do) */',
         table('MAP_PAD', pad, 160), table('MAP_BTN', btn, 12), table('MAP_KNOB', knob, 30),
         table('MAP_ENC', enc, 2)]
    open(out, 'w').write('\n'.join(h))
    print(out, len(d['links']), 'liens')


if __name__ == '__main__':
    main()
