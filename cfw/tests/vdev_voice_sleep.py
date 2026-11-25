#!/usr/bin/env python3
"""Voice Sleep (voice_sleep.c) : les voix de synthé VAP au repos ne sont plus calculées.

  * inactif au démarrage : toutes les voix VAP sont calculées (comme l'officiel),
  * Menu > Voice Sleep : message « Voice Sleep actif », les voix au repos sont sautées, la charge audio
    en silence baisse nettement (mode exact_count : 1 instruction = 1 cycle, ordres de grandeur),
  * une note envoyée à la voix 1 (événements de l'officiel : 4 note, 5 fin de note) la réveille : elle
    est calculée pendant la note et le relâchement, puis se rendort,
  * Menu > Voice Sleep à nouveau : inactif, la charge revient.

usage: vdev_voice_sleep.py <image.ppf>"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from device import CFW_RAM, Checks, open_device  # noqa: E402

SLEEP = CFW_RAM + 0xE8                      # struct sleep_state : magic, on, skipped, rendered
AUDIO = CFW_RAM + 0x10                      # struct audio_state (audio_load.c)
BUDGET = 1741497
MENU_SLEEP = 10
VOICE0 = 0x200423e0                         # objet de la 1re voix VAP (vtable 0x6008bae4) sur le virtuel
VOICE_EVENTS = 0x601316d1                   # slot 2 : aiguillage des événements (type en octet 0)
SCRATCH = 0x20240000                        # OCRAM libre (zone des invités, aucun chargé ici)


def st(vp):
    return [vp.r32(SLEEP + 4 * i) for i in range(4)]


def load(vp):
    return 100.0 * vp.r32(AUDIO + 4 * 9) / BUDGET      # avg_done (cycles par bloc, fenêtre d'1 s)


def texts(vp):
    return [a[2] for n, a in vp.eve.cmd_log if n == 'TEXT' and len(a) > 2]


def toggle(s):
    import flows
    s.vp.eve.cmd_log.clear()
    s.vp.eve.max_log = 200000
    flows.open_menu(s)
    flows.enter_item(s, MENU_SLEEP, after_ms=500)
    msg = [t for t in texts(s.vp) if t.startswith('Voice Sleep')]
    flows.close_menu(s)
    return msg[-1] if msg else ''


def event(vp, kind, note, vel):
    vp.mu.mem_write(SCRATCH + 16, b'\0' * 16)
    vp.mu.mem_write(SCRATCH, bytes([kind, 0, note, vel]))
    vp.call_fn(VOICE_EVENTS, VOICE0, SCRATCH + 16, SCRATCH)


def main():
    check = Checks()
    s = open_device(sys.argv[1])
    vp = s.vp
    import hle
    hle.enable_audio(vp)
    vp.exact_count(True)
    s.run_ms(1300)
    base = load(vp)
    check('inactif au démarrage : aucune voix sautée', st(vp)[1] == 0 and st(vp)[2] == 0, str(st(vp)))
    print(f'   charge sans Voice Sleep : {base:.1f} %')

    msg = toggle(s)
    check('Menu > Voice Sleep : « Voice Sleep actif »', msg == 'Voice Sleep actif' and st(vp)[1] == 1, msg)
    s.run_ms(2300)
    on = load(vp)
    sk, rd = st(vp)[2], st(vp)[3]
    check('en silence, les voix VAP au repos sont sautées', sk > 100 and rd == 0, f'sautées {sk}, calculées {rd}')
    check('la charge en silence baisse d’au moins 8 points', on < base - 8, f'{base:.1f} -> {on:.1f} %')
    print(f'   charge avec Voice Sleep : {on:.1f} %')

    r0 = st(vp)[3]
    event(vp, 4, 60, 100)                   # note : la voix passe à l'état 2
    s.run_ms(200)
    r1 = st(vp)[3]
    check('une note réveille la voix (calculée pendant la note)', r1 - r0 > 30 and vp.mu.mem_read(VOICE0 + 0x114, 1)[0] == 2,
          f'calculs {r1 - r0}, état {vp.mu.mem_read(VOICE0 + 0x114, 1)[0]}')
    event(vp, 5, 60, 0)                     # fin de note : relâchement (état 4) puis repos
    s.run_ms(100)
    check('relâchement : encore calculée', vp.mu.mem_read(VOICE0 + 0x114, 1)[0] in (0, 4) and st(vp)[3] > r1)
    s.run_ms(1000)
    r2 = st(vp)[3]
    s.run_ms(300)
    check('après le relâchement, la voix se rendort', st(vp)[3] == r2 and vp.mu.mem_read(VOICE0 + 0x114, 1)[0] == 0,
          f'{r2} -> {st(vp)[3]}')

    msg = toggle(s)
    check('Menu > Voice Sleep à nouveau : « Voice Sleep inactif »', msg == 'Voice Sleep inactif' and st(vp)[1] == 0, msg)
    s.run_ms(2300)
    off = load(vp)
    check('la charge revient', abs(off - base) < 3, f'{base:.1f} / {off:.1f} %')
    return check.done()


if __name__ == '__main__':
    sys.exit(main())
