#!/usr/bin/env python3
"""Cible minimale : Projects/globalSettings n'est plus réécrit à chaque cran de Master (settings_defer.c).

  * 10 crans de Shift + encodeur en 0,5 s : aucune écriture pendant le geste,
  * une seule écriture ~1,5 s après le dernier cran ; le fichier est identique à ce
    que l'officiel écrit lui-même pour cet état (appel direct de fw_global_settings_save). Le
    redémarrage du virtuel part d'un instantané pris après la lecture des réglages : on ne peut pas
    vérifier la relecture au démarrage ici, c'est à voir sur la machine,
  * au repos et sur un potard de piste : aucune écriture.

usage: TARGET=playplus-1.3.0-base scripts/test.sh vdev_settings_defer"""
import collections
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from device import Checks, cstr, open_device, work_dir  # noqa: E402
from unicorn.arm_const import UC_ARM_REG_R1  # noqa: E402

FILE_OPEN = 0x600aa8d0
MASTER = 0x2001d28c
GS = 0x20002728                    # struct gs_state : magic, pending, since, saves, deferred


def card_file(path='::/Projects/globalSettings'):
    r = subprocess.run(['mtype', '-i', os.path.join(work_dir(), 'sd.img'), path], capture_output=True,
                       env=dict(os.environ, MTOOLS_SKIP_CHECK='1'))
    return r.stdout if r.returncode == 0 else None


def watch(vp):
    c = collections.Counter()
    vp.hook_addr(FILE_OPEN, lambda vp: c.update([cstr(vp, vp.mu.reg_read(UC_ARM_REG_R1))]))
    return c


def main():
    check = Checks()
    s = open_device(sys.argv[1])
    vp = s.vp
    opens = watch(vp)
    s.run_ms(500)
    opens.clear()
    s.run_ms(1000)
    check('repos : aucune écriture', sum(opens.values()) == 0, str(dict(opens)))
    v0 = vp.r32(MASTER)
    f0 = card_file()
    vp.inputs.button(10, 0)
    s.run_ms(30)
    for _ in range(10):
        vp.inputs.encoder(-1)
        s.run_ms(50)
    vp.inputs.button(10, 1)
    s.run_ms(300)
    v1 = vp.r32(MASTER)
    check('le volume a bougé de 10 crans', v1 == v0 - 10, f'{v0}->{v1}')
    check('pendant le geste : aucune écriture de globalSettings', opens['Projects/globalSettings'] == 0,
          f'{dict(opens)} demandes {vp.r32(GS + 16)}')
    check('l’officiel a bien demandé à enregistrer', vp.r32(GS + 16) >= 1, str(vp.r32(GS + 16)))
    s.run_ms(1600)
    check('après le geste : une seule écriture', opens['Projects/globalSettings'] == 1, str(dict(opens)))
    f1 = card_file()
    # Constat (26/11) : le fichier (7 octets, format compact) ne contient pas le Master : il est le même
    # à 104 et à 94. L'officiel le réécrivait donc à chaque cran pour rien.
    check('le fichier est là', f1 is not None, repr(f1))
    vp.call_fn(0x600c848d, vp.profile['app'])      # l'officiel enregistre lui-même l'état courant
    s.run_ms(200)
    check('identique à ce qu’écrit l’officiel pour ce volume', card_file() == f1, f'{f1!r} / {card_file()!r}')
    opens.clear()
    for _ in range(10):
        vp.inputs.knob(11, 1)
        s.run_ms(50)
    s.run_ms(2000)
    check('Filter : aucune écriture', sum(opens.values()) == 0, str(dict(opens)))
    vp.inputs.button(10, 0)                 # remettre le volume d'origine pour les autres tests
    s.run_ms(30)
    vp.inputs.encoder(v0 - v1)
    s.run_ms(50)
    vp.inputs.button(10, 1)
    s.run_ms(2000)
    check('volume remis et enregistré', vp.r32(MASTER) == v0 and card_file() == f0, f'{vp.r32(MASTER)} {card_file()!r}')
    return check.done()


if __name__ == '__main__':
    sys.exit(main())
