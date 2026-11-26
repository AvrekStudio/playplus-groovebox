#!/usr/bin/env python3
"""Première matrice SMK-37 (Vector37) → Play+, proposée par Claude pour démarrer (le propriétaire la refait
dans la page de câblage). usage: default_map.py docs/correspondance/vector37.json"""
import json
import sys

BTN = [('KNOB BANK', 'Le guide (ouvre / ferme)'), ('FADER BANK', 'Jeu de potards MORE'),
       ('<', 'Son précédent ; liste : banque'), ('>', 'Son suivant ; liste : banque'),
       ('OCTAVE-', 'Octave −'), ('OCTAVE+', 'Octave +'), ('ARP', 'Jeu de potards ARP'),
       ('NOTE REPEAT', 'RESTART'), ('SCALE', 'Jeu de potards WAVE'), ('CHORD', 'Jeu de potards VOICE'),
       ('GLOBE', 'FOCUS ; tenu : off'), ('BT', 'Jeu de potards SETUP'),
       ('PATCH', 'Liste des sons (SOUNDS) ; deux fois : toutes les notes coupées'),
       ('PARA', 'RANDOM ; tenu : UNDO'), ('FX', 'Jeu de potards MIXER'), ('SEQ', 'FREEZE'),
       ('SEQ PLAY', 'TAP tempo'), ('SEQ REC', 'LATCH')]
KN = ['GEN 1', 'GEN 2', 'TUNE 2', 'BLEND', 'MODE', 'CUTOFF', 'RESO', 'LEVEL']
KN_ORBIT = ['WARP', 'WARP Y', 'ROTATE', 'CONTRAST', 'MOON DIST', 'SPIN', 'WIDTH', 'BPM']
FN = ['FAMILY', 'SIZE', 'SPEED', 'MORPH']
GN = ['SELECT', 'MODE', 'FILTER', 'MUTE']
NOTES = ['do', 'do#', 'ré', 'ré#', 'mi', 'fa', 'fa#', 'sol', 'sol#', 'la', 'la#', 'si']
BLACK = {1, 3, 6, 8, 10}
KNOB_NAME = ['Potard %d' % (i + 1) for i in range(15)]


def pad_label(i):
    r, c = divmod(i, 20)
    return 'Grille 4×8 r%d c%d' % (r, c - 16) if c >= 16 else 'Grille 16×8 r%d c%d' % (r, c)


def main():
    links = []

    def link(fm1, label, role, pp, ppl, **kw):
        e = {'fm1': fm1, 'fm1_label': label, 'fm1_role': role, 'playplus': pp, 'playplus_label': ppl,
             'shift': kw.pop('shift', False)}
        e.update(kw)
        links.append(e)

    def pad(fm1, label, role, i, **kw):
        r, c = divmod(i, 20)
        link(fm1, label, role, 'pad:%d' % i, pad_label(i), pad_index=i, pad_row=r, pad_col=c, **kw)

    # 16 pads du SMK : rangées 0 et 1 de la grille 16×8, colonnes 0 à 7 (comme sur le clavier : 2 × 8)
    for p in range(16):
        g, c = (p & 7) >> 1, (p & 1) + (2 if p >= 8 else 0) + 1
        pad('PAD%d' % p, 'Pad %d (%s %d)' % (p + 1, GN[g], c),
            'Groupe %s, coin %d de la planète' % (GN[g], c), (p // 8) * 20 + p % 8)
    # 37 touches (do à do) : blanches 1 à 16 en rangée 5, leurs noires en rangée 4 (colonne de la blanche
    # suivante, comme LOOM) ; blanches 17 à 22 en rangée 7, leurs noires en rangée 6
    w = 0
    for k in range(37):
        name = '%s%d' % (NOTES[k % 12], 2 + k // 12)
        if k % 12 in BLACK:
            row, col = (4, w) if w < 16 else (6, w - 16)
            pad('KEY%d' % k, 'noire %s' % name, 'Joue la note (vélocité fixe)', row * 20 + col)
        else:
            row, col = (5, w) if w < 16 else (7, w - 16)
            pad('KEY%d' % k, 'blanche %s' % name, 'Joue la note (vélocité fixe)', row * 20 + col)
            w += 1
    # K1..K8 : potards 1 à 8 ; faders : potards 11 à 14 ; roue mod : potard 15 ; pitch : potard 10
    for e in range(8):
        link('ENC%d' % e, 'K%d' % (e + 1), '%s (planète choisie) ; aucune planète : %s' % (KN[e], KN_ORBIT[e]),
             'knob:%d' % e, KNOB_NAME[e])
    for f in range(4):
        link('FADER%d' % f, 'Fader %d %s' % (f + 1, FN[f]), 'Le chemin : %s (valeur absolue, glisse)' % FN[f],
             'knob:%d' % (10 + f), KNOB_NAME[10 + f])
    link('WHEEL_MOD', 'Roue MOD', 'MOON SPEED : vitesse de la lune', 'knob:14', KNOB_NAME[14])
    link('WHEEL_PITCH', 'Roue PITCH', 'Pitch bend, 2 demi-tons (revient au centre)', 'knob:9', KNOB_NAME[9])
    # boutons du Play+
    for b, pp, ppl in [(3, 'btn:0', 'B0'), (2, 'btn:3', 'B3'), (16, 'btn:7', 'B7'), (17, 'btn:8', 'B8 Play'),
                       (15, 'btn:9', 'B9'), (12, 'btn:4', 'B4 Save'), (13, 'btn:5', 'B5 Copy'),
                       (14, 'btn:1', 'B1 Fill'), (10, 'btn:2', 'B2 Patterns'), (0, 'btn:11', 'B11 View')]:
        link('BTN%d' % b, BTN[b][0], BTN[b][1], pp, ppl)
    link('BTN3', BTN[3][0], BTN[3][1], 'enc', 'Encodeur (rotation)', action='cran+')
    link('BTN2', BTN[2][0], BTN[2][1], 'enc', 'Encodeur (rotation)', action='cran-')
    # les autres boutons sur la grille 4×8
    for b, i in [(4, 16), (5, 17), (6, 36), (9, 37), (8, 38), (1, 39), (7, 56), (11, 57)]:
        pad('BTN%d' % b, BTN[b][0], BTN[b][1], i)
    d = {'format': 'fm1-playplus-map/1', 'firmware': 'Vector37', 'firmware_id': 'vector37', 'links': links,
         'pad_colors': []}
    json.dump(d, open(sys.argv[1], 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
    open(sys.argv[1], 'a').write('\n')
    print(sys.argv[1], len(links), 'liens')


if __name__ == '__main__':
    main()
