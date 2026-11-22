#!/usr/bin/env python3
"""Socle : notre code est flashé, appelé et n'abîme pas l'officiel.

  * la fonction de version de l'officiel renvoie notre chaîne (CFW_VERSION_STRING),
  * notre crochet de boucle principale tourne : le compteur de passes avance,
  * la boucle n'est pas ralentie (plus long écart entre deux passes raisonnable),
  * le menu s'ouvre et se ferme normalement ; capture d'écran dans $VPLAY_DIR/shots.

usage: vdev_base.py <image.ppf>     (code de sortie 0 = tout passe)"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from device import CFW_RAM, VERSION_GETTER, Checks, cstr, expected_version, open_device, work_dir  # noqa: E402

LOOP_MAGIC = 0x41564B31


def loop_state(vp):
    return struct.unpack('<4I', bytes(vp.mu.mem_read(CFW_RAM, 16)))


def main():
    ppf = sys.argv[1]
    check = Checks()
    s = open_device(ppf)
    vp = s.vp
    shots = os.path.join(work_dir(), 'shots')
    os.makedirs(shots, exist_ok=True)

    v = cstr(vp, vp.call_fn(VERSION_GETTER))
    check('version = ' + expected_version(), v == expected_version(), repr(v))

    s.run_ms(100)
    magic, p0, _, _ = loop_state(vp)
    check('crochet de boucle actif (signature en RAM)', magic == LOOP_MAGIC, hex(magic))
    s.run_ms(1000)
    _, p1, _, gap = loop_state(vp)
    check('la boucle principale passe par notre code', p1 > p0, f'{p0} -> {p1}')
    check('pas de passe anormalement longue (< 200 ms)', gap < 200, f'{gap} ms')

    s.shot(os.path.join(shots, 'base_pattern.png'))
    import flows
    flows.open_menu(s)
    check('le menu s’ouvre', flows.current_screen(s) == 'menu', flows.current_screen(s))
    s.shot(os.path.join(shots, 'base_menu.png'))
    check('le menu se ferme', flows.close_menu(s))
    return check.done()


if __name__ == '__main__':
    sys.exit(main())
