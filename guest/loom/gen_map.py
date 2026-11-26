#!/usr/bin/env python3
"""Génère map_playplus.h (tables C) depuis une matrice « fm1-playplus-map/1 » (page de câblage).

usage : gen_map.py docs/correspondance/loom-8.json sortie/map_playplus.h

Chaque source du Play+ (pad 0..159, bouton 0..11, potard 0..14, encodeur) a deux couches : sans et avec
la touche Shift du Play+ (B10, qui n'est jamais transmise à LOOM). Une entrée dit quoi produire côté FM-1 :
  t   : MT_BTN (bouton du FM-1), MT_KEY (touche 0..26), MT_ENC (encodeur), MT_MASTER, ou MT_NONE ;
  id  : B_*, numéro de touche, ou EN_* ;
  act : MA_DIRECT, MA_INC (cran +), MA_DEC (cran -), MA_VAL (valeur val) ;
  mod : 0, ou 1 + B_* tenu pendant l'action (les combinaisons « HOME + ... » de la page).
"""
import json
import sys

BTN = ['B_FX', 'B_SEL', 'B_ENV', 'B_LFO', 'B_EDIT', 'B_GLO', 'B_HOME', 'B_SAVE', 'B_ARP', 'B_SEQ', 'B_PLAY',
       'B_REC', 'B_OCTDN', 'B_OCTUP']
ENC = ['EN_SELECT', 'EN_ALGO', 'EN_PRESET', 'EN_K1', 'EN_K2', 'EN_K3', 'EN_K4']
# noms des combinaisons de la page → identifiants FM-1
NAME = {'HOME': ('B', 'B_HOME'), 'REC': ('B', 'B_REC'), 'PLAY': ('B', 'B_PLAY'), 'SEQ': ('B', 'B_SEQ'),
        'SAVE': ('B', 'B_SAVE'), 'OCT+': ('B', 'B_OCTUP'), 'OCT-': ('B', 'B_OCTDN'), 'SELECT': ('E', 'EN_SELECT'),
        'PRESETS': ('E', 'EN_PRESET'), 'ALGORITHM': ('E', 'EN_ALGO'), 'KNOB1': ('E', 'EN_K1'), 'KNOB2': ('E', 'EN_K2'),
        'KNOB3': ('E', 'EN_K3'), 'KNOB4': ('E', 'EN_K4'),
        'EDIT': ('B', 'B_EDIT'), 'GLO': ('B', 'B_GLO'), 'FX': ('B', 'B_FX'), 'SEL': ('B', 'B_SEL'), 'ENV': ('B', 'B_ENV'),
        'LFO': ('B', 'B_LFO'), 'ARP': ('B', 'B_ARP')}


def target(fm1):
    """(t, id, mod) pour l'identifiant FM-1 de la matrice."""
    if fm1 in BTN:
        return 'MT_BTN', fm1, '0'
    if fm1 in ENC:
        return 'MT_ENC', fm1, '0'
    if fm1 == 'MASTER':
        return 'MT_MASTER', '0', '0'
    if fm1.startswith('KEY'):
        return 'MT_KEY', str(int(fm1[3:])), '0'
    if fm1.startswith('C:'):
        i = fm1.index('+', 3)                # (le 1er « + » après le 1er caractère : « OCT-+OCT+ »)
        mod, what = fm1[2:i], fm1[i + 1:]
        if what == '':
            raise ValueError(fm1)          # (« REC+OCT+ » se coupe bien en « REC » et « OCT+ »)
        m = NAME[mod]
        w = NAME[what]
        return ('MT_BTN' if w[0] == 'B' else 'MT_ENC'), w[1], '1 + ' + m[1]
    raise ValueError('commande FM-1 inconnue : ' + fm1)


def main():
    src, out = sys.argv[1], sys.argv[2]
    m = json.load(open(src))
    tabs = {'PAD': [[None] * 160, [None] * 160], 'BTN': [[None] * 12, [None] * 12],
            'KNOB': [[None] * 15, [None] * 15], 'ENC': [[None], [None]]}
    for l in m['links']:
        pp, layer = l['playplus'], 1 if l['shift'] else 0
        t, ident, mod = target(l['fm1'])
        a = l.get('action')
        act, val = 'MA_DIRECT', 0
        if a == 'cran+':
            act = 'MA_INC'
        elif a == 'cran-':
            act = 'MA_DEC'
        elif isinstance(a, dict):
            act, val = 'MA_VAL', int(a['value'])
        kind, _, num = pp.partition(':')
        key = {'pad': 'PAD', 'btn': 'BTN', 'knob': 'KNOB', 'enc': 'ENC'}[kind]
        idx = int(num) if num else 0
        if key == 'BTN' and idx == 10:
            raise ValueError('B10 (Shift du Play+) est réservé à la couche Shift')
        if tabs[key][layer][idx] is not None:
            raise ValueError(f'{pp} (couche {layer}) a deux commandes')
        tabs[key][layer][idx] = (t, ident, act, val, mod)
    colors = [None] * 160
    for c in m.get('pad_colors', []):
        h = c['color'].lstrip('#')
        colors[c['pad_index']] = tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))

    def row(e):
        return '{MT_NONE, 0, MA_DIRECT, 0, 0}' if e is None else '{%s, %s, %s, %d, %s}' % e

    lines = ['/* Généré par guest/loom/gen_map.py depuis %s (firmware %s) : NE PAS MODIFIER À LA MAIN. */'
             % (src.split('/')[-1], m.get('firmware', '?')),
             'enum { MT_NONE, MT_BTN, MT_KEY, MT_ENC, MT_MASTER };',
             'enum { MA_DIRECT, MA_INC, MA_DEC, MA_VAL };',
             'struct pp_map { uint8_t t, id, act, val, mod; };']
    for key, n in (('PAD', 160), ('BTN', 12), ('KNOB', 15), ('ENC', 1)):
        lines.append('static struct pp_map MAP_%s[2][%d] = {   /* (modifiable : map.json de la carte) */' % (key, n))
        for layer in (0, 1):
            lines.append('    {' + ', '.join(row(e) for e in tabs[key][layer]) + '},')
        lines.append('};')
    lines.append('static uint8_t MAP_PAD_RGB[160][4] = {   /* couleur choisie dans la page (4e = 1 si choisie) */')
    lines.append('    ' + ', '.join('{0, 0, 0, 0}' if c is None else '{%d, %d, %d, 1}' % c for c in colors))
    lines.append('};')
    open(out, 'w').write('\n'.join(lines) + '\n')


if __name__ == '__main__':
    main()
