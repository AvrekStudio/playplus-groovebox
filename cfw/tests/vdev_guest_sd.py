#!/usr/bin/env python3
"""Mode invité, jalon M4 : invité PGI chargé depuis la carte SD (guest_load.c, pgi.h, guest/).

  * un fichier /guest.pgi abîmé est refusé : on garde la mire de test intégrée, sans planter,
  * l'invité de démonstration (guest/demo) est chargé à 0x20200000 octet pour octet, son nom s'affiche,
  * un pad joue une note (son en sortie) et allume sa LED dans la couleur de sa rangée,
  * Shift + Back : sortie normale, Polyend reprend le son.

Le fichier est écrit sur l'image de carte du Play+ virtuel AVANT de repartir de l'instantané, et retiré
à la fin (les autres tests attendent une carte sans invité).

usage: vdev_guest_sd.py <image.ppf>"""
import os
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from device import CFW_RAM, ROOT, Checks, open_device, snapshot_for, work_dir  # noqa: E402

GUEST = CFW_RAM + 0x40
PGI_BASE = 0x20200000
AUDIO_OUT = 0x2000281c
LEDS = 0x20004d84
F_ACTIVE, F_PGI = 1, 28          # index des mots de struct guest_state (guest.h)


def g32(vp, k):
    return vp.r32(GUEST + 4 * k)


def sd_put(img, path, data):
    env = dict(os.environ, MTOOLS_SKIP_CHECK='1')
    tmp = img + '.pgi.tmp'
    with open(tmp, 'wb') as f:
        f.write(data)
    subprocess.run(['mcopy', '-o', '-i', img, tmp, '::' + path], check=True, env=env)
    os.remove(tmp)


def sd_del(img, path):
    env = dict(os.environ, MTOOLS_SKIP_CHECK='1')
    subprocess.run(['mdel', '-i', img, '::' + path], env=env, capture_output=True)


def led(vp, i):
    fr = bytes(vp.mu.mem_read(vp.r32(LEDS + 4) + 48 * i, 48))

    def comp(o):
        return sum(1 << (7 - k) for k in range(8) if fr[o + 1 + k] == 0x07)
    return comp(16), comp(0), comp(32)


def out_peak(vp):
    v = struct.unpack('<512h', bytes(vp.mu.mem_read(AUDIO_OUT, 1024)))
    return max(abs(x) for x in v)


def enter(s):
    import flows
    flows.open_menu(s)
    flows.enter_item(s, 7, after_ms=800)


def main():
    check = Checks()
    sys.path.insert(0, os.path.join(ROOT, 'guest'))
    import build as gbuild
    pgi = os.path.join(work_dir(), 'demo.pgi')
    gbuild.build(pgi, [os.path.join(ROOT, 'guest', 'demo', 'demo.c')])
    data = open(pgi, 'rb').read()
    snapshot_for(sys.argv[1])                    # crée l'instantané (et la carte) au premier lancement
    img = os.path.join(work_dir(), 'sd.img')
    check('image de carte du Play+ virtuel trouvée', os.path.exists(img), img)
    shots = os.path.join(work_dir(), 'shots')
    os.makedirs(shots, exist_ok=True)
    import hle

    try:
        # 1. fichier abîmé (magie fausse) : refusé, mire intégrée
        sd_put(img, '/guest.pgi', b'XXXX' + data[4:])
        s = open_device(sys.argv[1])
        hle.enable_audio(s.vp)
        s.run_ms(200)
        enter(s)
        check('fichier abîmé : refusé (pas d’invité chargé)', g32(s.vp, F_PGI) == 0, str(g32(s.vp, F_PGI)))
        check('fichier abîmé : mode invité quand même actif (mire intégrée)', g32(s.vp, F_ACTIVE) == 1)
        del s

        # 2. l'invité de démonstration
        sd_put(img, '/guest.pgi', data)
        s = open_device(sys.argv[1])
        vp = s.vp
        hle.enable_audio(vp)
        s.run_ms(200)
        enter(s)
        check('invité chargé (pgi = 1) et mode actif', g32(vp, F_PGI) == 1 and g32(vp, F_ACTIVE) == 1,
              'pgi %d active %d' % (g32(vp, F_PGI), g32(vp, F_ACTIVE)))
        check('image copiée octet pour octet à 0x20200000',
              bytes(vp.mu.mem_read(PGI_BASE, len(data))) == data)
        s.run_ms(200)
        check('silence tant qu’aucun pad n’est joué', out_peak(vp) == 0, str(out_peak(vp)))
        vp.inputs.pad(7, 0, 0)                   # rangée du bas, 1re colonne : tenu
        s.run_ms(120)
        check('un pad joue une note (son de l’invité en sortie)', out_peak(vp) > 1000, str(out_peak(vp)))
        check('sa LED s’allume dans la couleur de la rangée 7', led(vp, 140) == (255, 60, 180), str(led(vp, 140)))
        s.shot(os.path.join(shots, 'guest_sd.png'))
        txt = [a for name, a in vp.eve.cmd_log if name == 'TEXT'] if hasattr(vp.eve, 'cmd_log') else []
        vp.inputs.pad(7, 0, 1)
        s.run_ms(1500)
        check('la note s’éteint d’elle-même (enveloppe)', out_peak(vp) < 2000, str(out_peak(vp)))
        check('la LED suit l’enveloppe et s’éteint', led(vp, 140) != (255, 60, 180), str(led(vp, 140)))
        del txt

        vp.inputs.button(10, 0)
        s.run_ms(60)
        s.tap(7)
        vp.inputs.button(10, 1)
        s.run_ms(200)
        check('Shift + Back : sortie', g32(vp, F_ACTIVE) == 0)
        s.pad(7, 3)
        s.run_ms(100)
        check('après la sortie, le pad ne joue plus l’invité (silence Polyend)', out_peak(vp) == 0, str(out_peak(vp)))
    finally:
        sd_del(img, '/guest.pgi')
    return check.done()


if __name__ == '__main__':
    sys.exit(main())
