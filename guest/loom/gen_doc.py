#!/usr/bin/env python3
"""Fiche lisible d'une matrice FM-1 → Play+ (format fm1-playplus-map/1), à côté du JSON.

usage: gen_doc.py docs/correspondance/loom-8.json [docs/correspondance/LOOM-8.md]
Le JSON vient de la page « Câblage FM-1 → Play+ » ; la fiche se régénère à chaque nouvelle matrice."""
import json
import os
import sys


def where(l):
    p = l['playplus_label']
    if l.get('shift'):
        p = 'Shift + ' + p
    a = l.get('action')
    if isinstance(a, dict) and 'value' in a:
        p += ' — valeur %d' % a['value']
    elif a == 'cran+':
        p += ' — cran +'
    elif a == 'cran-':
        p += ' — cran −'
    return p


def row(l):
    return '| %s | %s | %s |' % (l['fm1_label'], where(l), l.get('fm1_role', ''))


def main():
    src = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(src), json.load(open(src))['firmware'] + '.md')
    d = json.load(open(src, encoding='utf-8'))
    links = d['links']
    btn = [l for l in links if l['fm1'].startswith('B_')]
    enc = [l for l in links if l['fm1'].startswith('EN_') or l['fm1'] == 'MASTER']
    combo = [l for l in links if l['fm1'].startswith('C:')]
    keys = sorted([l for l in links if l['fm1'].startswith('KEY')], key=lambda l: l['fm1_key'])
    head = '| FM-1 | Play+ | Rôle dans %s |\n|---|---|---|' % d['firmware']
    used_btn = {int(l['playplus'][4:]) for l in links if l['playplus'].startswith('btn:')}
    used_knob = {int(l['playplus'][5:]) for l in links if l['playplus'].startswith('knob:')}
    free_btn = [b for b in range(12) if b not in used_btn and b != 10]
    free_knob = [k + 1 for k in range(15) if k not in used_knob]
    t = ['# Correspondance FM-1 → Play+ : %s' % d['firmware'], '',
         'Matrice du propriétaire, faite dans la page « Câblage FM-1 → Play+ » ; fichier source : '
         '`%s` (format `%s`). Fiche générée par `guest/loom/gen_doc.py` : ne pas la modifier à la main.'
         % (os.path.basename(src), d['format']), '',
         '## Comment la lire', '',
         '- Grille 16×8 : colonnes c0 à c15 ; grille 4×8 : ses colonnes c0 à c3 (c16 à c19 au total) ; r0 = rangée du haut.',
         '- Potards du Play+ : 1 à 5 = rangée du haut, 6 à 10 = milieu, 11 à 15 = bas.',
         '- « cran + / cran − » : un appui = un cran de l’encodeur du FM-1 vers la droite / la gauche.',
         '- « valeur N » : l’encodeur est amené à la position N (pour ALGORITHM : la piste N).',
         '- Combinaisons : l’action du Play+ produit, côté firmware, HOME tenu + le contrôle nommé.',
         '- Le Shift du Play+ (B10) est une couche à nous, jamais transmise au firmware.', '',
         '## Boutons', '', head] + [row(l) for l in btn] + [
         '', '## Encodeurs et MASTER', '', head] + [row(l) for l in enc] + [
         '', '## Combinaisons (HOME tenu)', '', head] + [row(l) for l in combo] + [
         '', '## Clavier', '', head] + [row(l) for l in keys] + [
         '', '## Libres', '',
         '- Boutons : ' + (', '.join('B%d' % b for b in free_btn) or 'aucun')
         + (' (B6 = clic de l’encodeur : Shift + B6 tenu 2 s = sortie d’Avrek OS)' if 6 in free_btn else ''),
         '- Potards : ' + (', '.join(str(k) for k in free_knob) or 'aucun'), '']
    open(out, 'w', encoding='utf-8').write('\n'.join(t))
    print(out)


if __name__ == '__main__':
    main()
