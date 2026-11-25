#!/usr/bin/env python3
"""Cible minimale : potard 1 (index 0) libéré, Master Volume sur Shift + encodeur (master_shift.c).

  * tourner ou toucher le potard 0 ne change ni le volume ni le paramètre courant,
  * Shift tenu + encodeur : le volume général bouge, dans le bon sens,
  * encodeur sans Shift : le volume ne bouge pas,
  * les autres potards marchent comme avant.

usage: TARGET=playplus-1.3.0-base scripts/test.sh vdev_master_shift"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from device import Checks, cstr, expected_version, open_device  # noqa: E402

VERSION_PTR = 0x600c83f0
MASTER = 0x2001d28c                # volume général observé (master_shift.c), 104 au départ
MS = 0x20002710                    # struct ms_state : magic, shift, dropped, routed
BTN_SHIFT = 10


def texts(vp):
    return [a[2] for n, a in vp.eve.cmd_log if n == 'TEXT' and len(a) > 2]


def main():
    check = Checks()
    s = open_device(sys.argv[1])
    vp = s.vp
    v = cstr(vp, vp.r32(VERSION_PTR))
    check('version = ' + expected_version(), v == expected_version(), repr(v))
    ctrl = vp.profile['app'] + 0x38
    cur = lambda: vp.r32(ctrl + 0x240c)     # noqa: E731
    vol = lambda: vp.r32(MASTER)            # noqa: E731
    s.run_ms(300)
    s.knob(6, 1, after_ms=400)
    check('potard 7 (Volume / Pan) choisi', cur() == 6, str(cur()))
    v0 = vol()

    s.knob(0, -3, after_ms=400)
    s.knob(0, 2, after_ms=400)
    s.touch(0)
    check('potard 1 : ni volume ni paramètre ne bougent', vol() == v0 and cur() == 6,
          f'vol {v0}->{vol()} param {cur()}')
    check('potard 1 : évènements avalés', vp.r32(MS + 8) >= 4, str(vp.r32(MS + 8)))

    s.turn(-2, after_ms=400)
    check('encodeur seul : le volume ne bouge pas', vol() == v0, f'{v0}->{vol()}')

    vp.eve.cmd_log.clear()
    vp.eve.max_log = 200000
    vp.inputs.button(BTN_SHIFT, 0)
    s.run_ms(80)
    vp.inputs.button(BTN_SHIFT, 2)
    s.turn(-2, after_ms=400)
    v1 = vol()
    s.turn(1, after_ms=400)
    v2 = vol()
    vp.inputs.button(BTN_SHIFT, 1)
    s.run_ms(400)
    t = texts(vp)
    check('Shift + encodeur vers la gauche : volume plus bas', v1 < v0, f'{v0}->{v1}')
    check('Shift + encodeur vers la droite : volume plus haut', v2 > v1, f'{v1}->{v2}')
    check('crans envoyés au volume', vp.r32(MS + 12) >= 2, str(vp.r32(MS + 12)))
    check('l’écran montre le volume en dB', any('dB' in x for x in t), str([x for x in t if 'B' in x][:6]))
    check('Shift relâché', vp.r32(MS + 4) == 0)
    s.turn(1, after_ms=400)
    check('après Shift : l’encodeur ne touche plus au volume', vol() == v2, f'{v2}->{vol()}')
    check('après Shift : le paramètre d’avant est revenu (Volume / Pan)', cur() == 6, str(cur()))
    s.knob(11, 1, after_ms=400)
    check('les autres potards marchent (Filter)', cur() == 11, str(cur()))
    d = os.path.join(os.environ.get('VPLAY_DIR', '.'), 'shots')
    os.makedirs(d, exist_ok=True)
    s.shot(os.path.join(d, 'master_shift.png'))
    return check.done()


if __name__ == '__main__':
    sys.exit(main())
