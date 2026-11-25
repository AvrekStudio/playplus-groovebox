#!/usr/bin/env python3
"""Cible minimale playplus-1.3.0-base : l'officiel 1.3.0 + le réglage Knob Touch (knob_touch.c).

  * la version affichée est la nôtre, le menu s'ouvre et se ferme,
  * Menu > Settings se termine par « Knob Touch  On »,
  * On : toucher un potard en fait le paramètre courant (comme l'officiel),
  * Off (choisi dans Settings) : toucher ne change rien, tourner choisit toujours le paramètre, pas de
    drapeau de toucher resté levé, le fichier /Projects/avrekKnobTouch vaut 1,
  * après un redémarrage complet (instantané + carte), Off est toujours là.

usage: TARGET=playplus-1.3.0-base scripts/test.sh"""
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from device import Checks, cstr, expected_version, open_device, work_dir  # noqa: E402

MENU_SETTINGS = 7                  # File Samples LFO Matrix Synths Scales MIDI Settings
LISTENER = 0x20018c34
VERSION_PTR = 0x600c83f0
KT = 0x20002700                    # struct kt_state : magic, loaded, off, dropped
ENV = dict(os.environ, MTOOLS_SKIP_CHECK='1')


def texts(vp):
    return [a[2] for n, a in vp.eve.cmd_log if n == 'TEXT' and len(a) > 2]


def pair(t, a, b):
    return any(t[i] == a and t[i + 1] == b for i in range(len(t) - 1))


def settings_count(vp):
    scr = vp.r32(vp.r32(LISTENER + 8) + 0x10)
    return vp.r32(vp.r32(scr + 0x2c) + 0x0c)


def card_file(img):
    r = subprocess.run(['mtype', '-i', img, '::/Projects/avrekKnobTouch'], capture_output=True, env=ENV)
    return r.stdout if r.returncode == 0 else None


def main():
    check = Checks()
    import flows
    img = os.path.join(work_dir(), 'sd.img')
    s = open_device(sys.argv[1])
    vp = s.vp
    subprocess.run(['mdel', '-i', img, '::/Projects/avrekKnobTouch'], capture_output=True, env=ENV)
    v = cstr(vp, vp.r32(VERSION_PTR))
    check('version = ' + expected_version(), v == expected_version(), repr(v))
    ctrl = vp.profile['app'] + 0x38
    cur = lambda: vp.r32(ctrl + 0x240c)     # noqa: E731  paramètre courant du contrôleur (FreePlay, screen.c)
    s.run_ms(300)

    flows.open_menu(s)
    check('le menu s’ouvre', flows.current_screen(s) == 'menu', flows.current_screen(s))
    flows.enter_item(s, MENU_SETTINGS)
    vp.eve.cmd_log.clear()
    vp.eve.max_log = 200000
    s.turn(64, after_ms=400)
    t = texts(vp)
    check('Settings se termine par « Knob Touch  On »', pair(t, 'Knob Touch', 'On'), str(t[-8:]))
    n = settings_count(vp)
    s.shot(os.path.join(work_dir(), 'shots', 'knob_touch_settings.png')) if os.path.isdir(os.path.join(work_dir(), 'shots')) else None
    check('le menu se ferme', flows.close_menu(s))

    s.touch(3)
    check('On : toucher le potard 4 le choisit', cur() == 3, str(cur()))
    s.knob(1, 1, after_ms=300)
    check('tourner le potard 2 le choisit', cur() == 1, str(cur()))

    flows.open_menu(s)
    flows.enter_item(s, MENU_SETTINGS)
    flows.enter_item(s, n - 1)              # Knob Touch : la liste des choix
    s.turn(1, after_ms=300)
    vp.eve.cmd_log.clear()
    s.tap(flows.BTN_ENTER, after_ms=600)    # Off
    check('« Knob Touch  Off »', pair(texts(vp), 'Knob Touch', 'Off') and vp.r32(KT + 8) == 1,
          f'{texts(vp)[-6:]} off={vp.r32(KT + 8)}')
    flows.close_menu(s)
    s.touch(4)
    check('Off : toucher le potard 5 ne change rien', cur() == 1 and vp.r32(KT + 12) > 0, f'{cur()} avalés {vp.r32(KT + 12)}')
    vp.inputs.touch(5, 0)
    s.knob(5, 1, after_ms=300)
    vp.inputs.touch(5, 1)
    s.run_ms(100)
    check('Off : tourner choisit toujours le paramètre', cur() == 5, str(cur()))
    check('Off : pas de drapeau de toucher resté levé',
          all(vp.mu.mem_read(ctrl + 0x2455 + k, 1)[0] == 0 for k in range(14)))
    s.run_ms(300)
    del s, vp
    check('fichier /Projects/avrekKnobTouch = 1', card_file(img) == b'\x01', repr(card_file(img)))

    s = open_device(sys.argv[1])            # redémarrage complet : instantané de démarrage + même carte
    vp = s.vp
    ctrl = vp.profile['app'] + 0x38
    s.run_ms(300)
    s.knob(2, 1, after_ms=300)
    s.touch(7)
    check('après redémarrage : Off est toujours actif', vp.r32(ctrl + 0x240c) == 2, str(vp.r32(ctrl + 0x240c)))
    subprocess.run(['mdel', '-i', img, '::/Projects/avrekKnobTouch'], capture_output=True, env=ENV)
    return check.done()


if __name__ == '__main__':
    sys.exit(main())
