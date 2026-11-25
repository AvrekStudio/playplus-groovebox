#!/usr/bin/env python3
"""Kit d'arrivée de la machine : Menu > Explorateur et Menu > Auto-test (guest_test.c).

  * l'explorateur s'ouvre sans invité (même avec des invités sur la carte : pas de sélecteur),
  * chaque commande touchée s'inscrit au journal avec son numéro et son état (pad, bouton, potard,
    encodeur), et s'affiche à l'écran,
  * l'auto-test : les tests d'exécution en OCRAM et en SDRAM passent (le Play+ virtuel exécute partout :
    sur la machine, c'est ce qu'on veut savoir), tous les tests ont un résultat, la ligne s'affiche,
  * un auto-test qui a « planté » au lancement précédent (marque laissée en RAM) est signalé PLANTE et
    n'est pas relancé.

usage: vdev_guest_explorer.py <image.ppf>"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from device import CFW_RAM, Checks, clean_card, open_device, work_dir  # noqa: E402

GUEST = CFW_RAM + 0x40
F_ACTIVE, F_PICKING, F_PGI = 1, 29, 28
F_LOG, F_HEAD, F_EXPLORER, F_RES, F_STEP = 34, 38, 39, 40, 41
MENU_EXPLORER, MENU_SELFTEST = 8, 9
TEST_MARK = 0x54535400


def g32(vp, k):
    return vp.r32(GUEST + 4 * k)


def last_event(vp):
    e = g32(vp, F_LOG + ((g32(vp, F_HEAD) - 1) & 3))
    b = e >> 16
    return e & 0xF, (e >> 8) & 0xFF, b - 0x10000 if b & 0x8000 else b, (e >> 4) & 0xF


def texts(vp):
    return [a[2] for n, a in vp.eve.cmd_log if n == 'TEXT' and len(a) > 2]


def exit_guest(s):
    s.vp.inputs.button(10, 0)
    s.run_ms(60)
    s.tap(7)
    s.vp.inputs.button(10, 1)
    s.run_ms(200)


def main():
    check = Checks()
    clean_card(sys.argv[1])
    import flows
    s = open_device(sys.argv[1])
    vp = s.vp
    shots = os.path.join(work_dir(), 'shots')
    os.makedirs(shots, exist_ok=True)
    s.run_ms(200)

    # 1. explorateur
    flows.open_menu(s)
    flows.enter_item(s, MENU_EXPLORER, after_ms=800)
    check('Explorateur : mode invité actif, sans invité ni sélecteur',
          g32(vp, F_ACTIVE) == 1 and g32(vp, F_EXPLORER) == 1 and g32(vp, F_PICKING) == 0 and g32(vp, F_PGI) == 0)
    s.pad(3, 10)
    check('pad (3, 10) au journal : n° 70, relâché en dernier', last_event(vp) == (0, 70, 1, 1), str(last_event(vp)))
    s.tap(8)
    check('bouton Play au journal : n° 8', last_event(vp)[:2] == (1, 8), str(last_event(vp)))
    s.knob(4, -3)
    check('potard 4, -3 au journal', last_event(vp)[:3] == (2, 4, -3), str(last_event(vp)))
    s.turn(2)
    check('encodeur +2 au journal', last_event(vp)[0] == 4 and last_event(vp)[2] == 2, str(last_event(vp)))
    vp.eve.cmd_log.clear()
    vp.eve.max_log = 200000
    s.run_ms(100)
    t = texts(vp)
    check('écran : titre et journal (« Encodeur +2 », « Potard 4 -3 »)',
          'EXPLORATEUR' in t and 'Encodeur +2' in t and 'Potard 4 -3' in t, str(t[:8]))
    s.shot(os.path.join(shots, 'explorateur.png'))
    exit_guest(s)
    check('Shift + Back : sortie', g32(vp, F_ACTIVE) == 0)

    # 2. auto-test
    flows.open_menu(s)
    flows.enter_item(s, MENU_SELFTEST, after_ms=800)
    res = g32(vp, F_RES)
    r = [(res >> (2 * i)) & 3 for i in range(4)]
    check('Auto-test : les 4 tests ont un résultat', all(x != 0 for x in r), str(r))
    check('Auto-test : exécution en OCRAM et en SDRAM réussie (virtuel)', r[2] == 1 and r[3] == 1, str(r))
    check('Auto-test : pas de marque de plantage laissée', g32(vp, F_STEP) == 0)
    vp.eve.cmd_log.clear()
    s.run_ms(100)
    line = [x for x in texts(vp) if x.startswith('MPU ') or x.startswith('OCRAM ')]
    check('Auto-test : résumé affiché dans l’explorateur', bool(line), str(texts(vp)[:6]))
    if line:
        print('   résumé :', ' | '.join(dict.fromkeys(line)))
    s.shot(os.path.join(shots, 'autotest.png'))
    exit_guest(s)

    # 3. un test qui avait planté
    vp.mu.mem_write(GUEST + 4 * F_STEP, (TEST_MARK | 2).to_bytes(4, 'little'))
    vp.mu.mem_write(0x20200000, b'\0' * 4)
    flows.open_menu(s)
    flows.enter_item(s, MENU_SELFTEST, after_ms=800)
    res = g32(vp, F_RES)
    check('test resté marqué : OCRAM signalé PLANTE, non relancé',
          (res >> 4) & 3 == 3 and bytes(vp.mu.mem_read(0x20200000, 4)) == b'\0' * 4, hex(res))
    exit_guest(s)
    return check.done()


if __name__ == '__main__':
    sys.exit(main())
