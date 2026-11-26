#!/usr/bin/env python3
"""Écrit la matrice par défaut de FM1 Tonic : docs/correspondance/tonic.json (format de la page de câblage,
« fm1-playplus-map/1 »). À rouvrir dans la page pour la retoucher.

Tonic est une boîte à rythmes à pas : on pose son clavier « à plat » sur la grille 16×8 du Play+ :
  rangée 7 : les 16 touches blanches = steps 1 à 16 (blanc chaud) ;
  rangée 6 : les noires TRACK 1 à 8 (colonnes 0-7, bleu), ACCENT (12, bleu foncé), FILL (13, jaune), FUNC (15, rouge) ;
  rangée 0 : les boutons de pages, aux couleurs de la façade (FX… GLO, HOME… SEQ), OCT- / OCT+ (14, 15) ;
  potards : 1 MASTER, 2-5 KNOB 1-4, 6 ALGORITHM, 7 PRESETS ; encodeur : SELECT ;
  boutons : B8 PLAY, B9 REC, B4 SAVE, B2 SEQ, B1 FILL, B7 HOME, B5 GLO, B6 (clic) OCT+ (OUI), B3 OCT- (NON),
  B0 FX, B11 ARP. B10 reste le Shift du Play+ (couche libre).

usage : guest/tonic/default_map.py [sortie.json]"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', '..', 'docs', 'correspondance', 'tonic.json')
WHITE = [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26]
BLACK = [1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25]          # TRACK 1-8, ACCENT, FILL, FUNC
TEAL, BLUE, PURPLE, GREY, BROWN, GREEN, RED = '#45b3bd', '#6f93e0', '#b080d8', '#8a90a0', '#d9a040', '#5cc464', '#ff6a60'
ROW0 = [(0, 'B_FX', TEAL), (1, 'B_SEL', BLUE), (2, 'B_ENV', BLUE), (3, 'B_LFO', BLUE), (4, 'B_EDIT', BLUE),
        (5, 'B_GLO', PURPLE), (8, 'B_HOME', TEAL), (9, 'B_SAVE', GREY), (10, 'B_ARP', BROWN), (11, 'B_SEQ', BROWN),
        (14, 'B_OCTDN', GREY), (15, 'B_OCTUP', GREY)]
BTNS = {8: 'B_PLAY', 9: 'B_REC', 4: 'B_SAVE', 2: 'B_SEQ', 1: 'KEY22', 7: 'B_HOME', 5: 'B_GLO', 6: 'B_OCTUP',
        3: 'B_OCTDN', 0: 'B_FX', 11: 'B_ARP'}
KNOBS = {0: 'MASTER', 1: 'EN_K1', 2: 'EN_K2', 3: 'EN_K3', 4: 'EN_K4', 5: 'EN_ALGO', 6: 'EN_PRESET'}


def main(out):
    links, colors = [], []

    def pad(r, c, fm1, color):
        i = r * 20 + c
        links.append({'fm1': fm1, 'playplus': f'pad:{i}', 'shift': False, 'pad_index': i, 'pad_row': r, 'pad_col': c})
        colors.append({'pad_index': i, 'pad_row': r, 'pad_col': c, 'color': color})
    for c, k in enumerate(WHITE):
        pad(7, c, f'KEY{k}', '#ffdcb4')
    for c, k in enumerate(BLACK[:8]):
        pad(6, c, f'KEY{k}', '#3b82d6')
    pad(6, 12, f'KEY{BLACK[8]}', '#1c3f9a')
    pad(6, 13, f'KEY{BLACK[9]}', '#e0b000')
    pad(6, 15, f'KEY{BLACK[10]}', '#c0281c')
    for c, b, col in ROW0:
        pad(0, c, b, col)
    for b, fm1 in sorted(BTNS.items()):
        links.append({'fm1': fm1, 'playplus': f'btn:{b}', 'shift': False})
    for k, fm1 in sorted(KNOBS.items()):
        links.append({'fm1': fm1, 'playplus': f'knob:{k}', 'shift': False})
    links.append({'fm1': 'EN_SELECT', 'playplus': 'enc', 'shift': False})
    m = {'format': 'fm1-playplus-map/1', 'firmware': 'FM1 Tonic', 'firmware_id': 'tonic', 'links': links,
         'pad_colors': colors}
    with open(out, 'w', encoding='utf-8') as f:
        json.dump(m, f, ensure_ascii=False, indent=1)
    print(out, len(links), 'liens')


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else OUT)
