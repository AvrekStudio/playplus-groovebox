#!/usr/bin/env python3
"""Menu > Diagnostic (menu.c) sur le Play+ virtuel.

  * le menu principal garde ses 6 lignes d'origine, dans l'ordre, et gagne « Diagnostic » en 7e,
  * Diagnostic affiche un message avec la charge audio (« Audio xx.x% pic yy.y% ») une fois la
    première fenêtre d'1 s mesurée,
  * captures : diag_menu.png, diag_toast.png dans $VPLAY_DIR/shots.

usage: vdev_diag.py <image.ppf>"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from device import Checks, open_device, work_dir  # noqa: E402

STOCK_ROOT = ['File', 'Samples', 'Synths', 'Scales', 'MIDI', 'Settings']


def texts_since(vp, n0):
    return [a[2] for n, a in vp.eve.cmd_log[n0:] if n == 'TEXT' and len(a) > 2]


def fresh_log(vp):
    """Le journal des commandes EVE s'arrête à max_log entrées (déjà atteint après le démarrage) :
    on le vide avant chaque observation."""
    vp.eve.cmd_log.clear()
    vp.eve.max_log = 200000
    return 0


def main():
    check = Checks()
    s = open_device(sys.argv[1])
    vp = s.vp
    shots = os.path.join(work_dir(), 'shots')
    os.makedirs(shots, exist_ok=True)
    import flows
    import hle
    hle.enable_audio(vp)
    vp.exact_count(True)
    s.run_ms(1300)                       # au moins une fenêtre d'1 s mesurée

    n0 = fresh_log(vp)
    flows.open_menu(s)
    s.turn(64, after_ms=300)             # descendre jusqu'en bas : toutes les lignes passent à l'écran
    rows = [t for t in texts_since(vp, n0) if t in STOCK_ROOT + ['Diagnostic']]
    seen = list(dict.fromkeys(rows))
    check('menu d’origine intact, Diagnostic ajouté en dernier', seen == STOCK_ROOT + ['Diagnostic'], str(seen))
    s.shot(os.path.join(shots, 'diag_menu.png'))

    n1 = fresh_log(vp)
    flows.enter_item(s, 6, after_ms=500)
    msgs = [t for t in texts_since(vp, n1) if t.startswith('Audio')]
    s.shot(os.path.join(shots, 'diag_toast.png'))
    ok = bool(msgs) and re.fullmatch(r'Audio \d+\.\d% pic \d+\.\d%', msgs[-1]) is not None
    check('Diagnostic affiche la charge audio', ok, str(msgs[-3:]))
    if msgs:
        print('   message :', msgs[-1])
    check('on ressort du menu normalement', flows.close_menu(s))
    return check.done()


if __name__ == '__main__':
    sys.exit(main())
