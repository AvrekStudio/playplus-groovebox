#!/usr/bin/env python3
"""Cible playplus-1.3.0-own (Avrek OS 0.6.2) : le banc d'essai (guest/banc) sur le Play+ virtuel.

  * BANC.pgi (ABI 5, grande zone SDRAM) listé, chargé ;
  * il passe tout seul PROCESSEUR, SON AU REPOS, SON + ECRAN, CARTE SD, puis attend les gestes ;
  * gestes simulés : potard 1, pads avec un 3e argument (PGI_EV_PADX), toucher (PGI_EV_TOUCH), encodeur, PLAY ;
  * /Guests/BANC/rapport.txt écrit, avec les sections attendues, la lecture par morceaux (fopen / fread de l'ABI 5)
    et le 3e argument des pads relayé. Les chiffres du virtuel ne valent rien (pas de vrai temps) : seule la
    machine donne les vrais.

usage: TARGET=playplus-1.3.0-own scripts/test.sh vdev_own_banc       ($BANC_PGI : un .pgi déjà construit)"""
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'vdev'))
from device import CFW_RAM, ROOT, Checks, clean_card, cstr, open_device, snapshot_for, work_dir  # noqa: E402
from vdev_guest_sd import sd_put  # noqa: E402
from vdev_own_loom import O  # noqa: E402
import hle  # noqa: E402


def main():
    check = Checks()
    clean_card(sys.argv[1])
    wd = work_dir()
    pgi = os.environ.get('BANC_PGI')
    if not pgi:
        sys.path.insert(0, os.path.join(ROOT, 'guest', 'banc'))
        import build_banc
        pgi = build_banc.build(os.path.join(wd, 'banc.pgi'))
    data = open(pgi, 'rb').read()
    snapshot_for(sys.argv[1])
    img = os.path.join(wd, 'sd.img')
    d = os.path.join(wd, 'shots')
    os.makedirs(d, exist_ok=True)
    env = dict(os.environ, MTOOLS_SKIP_CHECK='1')
    subprocess.run(['mmd', '-i', img, '::/Guests'], env=env, capture_output=True)
    subprocess.run(['mdeltree', '-i', img, '::/Guests/BANC'], env=env, capture_output=True)
    try:
        sd_put(img, '/Guests/BANC.pgi', data)
        s = open_device(sys.argv[1])
        vp = s.vp
        hle.enable_audio(vp)
        st, m = 0, 0
        for _ in range(60):
            st = vp.r32(CFW_RAM)
            m = st and vp.r32(st + 4) == 1 and vp.r32(st + O['menu'])
            if m and vp.r32(m) == 1:
                break
            s.run_ms(200)
        r = lambda k: vp.r32(st + O[k])                   # noqa: E731
        check('menu : BANC listé', m and cstr(vp, m + 8) == 'BANC.pgi', cstr(vp, m + 8) if m else '-')
        s.tap(6, after_ms=100)
        for _ in range(60):
            if r('pgi') and r('ready'):
                break
            s.run_ms(200)
        check('BANC chargé (grande zone SDRAM, ABI 5 ou plus), init finie', r('pgi') == 0x81800000 and r('ready') == 1
              and vp.r32(0x81800004) >= 5, '%#x abi %d' % (r('pgi'), vp.r32(0x81800004) if r('pgi') else -1))
        s.run_ms(500)
        s.vp.eve.screenshot(os.path.join(d, 'banc_0.png'), 2)

        def rapport():
            return subprocess.run(['mtype', '-i', img, '::/Guests/BANC/rapport.txt'], env=env, capture_output=True,
                                  text=True).stdout
        txt = ''
        for _ in range(240):                              # jusqu'à la demande du potard (les mesures automatiques)
            s.run_ms(250)
            txt = rapport()
            if 'POTARD 1' in txt:
                break
        s.vp.eve.screenshot(os.path.join(d, 'banc_1.png'), 2)
        for sec in ('== PROCESSEUR', '== MEMOIRE', 'depuis SDRAM', 'depuis OCRAM', 'SON au repos', 'SON + ecran',
                    '== CARTE SD', 'relire 1 Mo', 'SON pendant la carte'):
            check('rapport : « %s »' % sec, sec in txt, txt[-400:])
        check('carte : 1 Mo relu sans octet faux', 'octets faux: 0' in txt)
        check('ABI 5 : lecture par morceaux (fopen / fread)', 'morceaux de 4 Ko' in txt and 'morceaux de 32 Ko' in txt
              and 'indisponible' not in txt)
        # les gestes
        for _ in range(6):
            s.knob(0, 5, after_ms=50)
        for _ in range(3):
            s.knob(0, -4, after_ms=50)
        s.tap(8, after_ms=600)
        for c in (12, 90, 127):                            # pads avec un 3e argument (r3 du listener)
            vp.inputs.queue.append((vp.inputs.slots['pad'], (7 * 20 + 3, 0, c)))
            s.run_ms(60)
            vp.inputs.queue.append((vp.inputs.slots['pad'], (7 * 20 + 3, 1, 0)))
            s.run_ms(60)
        s.tap(8, after_ms=600)
        s.touch(4, hold_ms=100, after_ms=200)
        s.touch(4, hold_ms=100, after_ms=200)
        s.tap(8, after_ms=600)
        s.turn(7, after_ms=200)
        s.tap(8, after_ms=1500)
        txt = rapport()
        s.vp.eve.screenshot(os.path.join(d, 'banc_2.png'), 2)
        check('potard 1 : crans cumulés 30, puis 18 (min..max 0..30)', 'P1: 0..30' in txt, txt[-900:])
        check('ABI 5 : 3e argument des pads relayé (PGI_EV_PADX)', '143:12' in txt and '143:90' in txt and
              '143:127' in txt and 'max 127' in txt)
        check('ABI 5 : toucher des potards relayé (PGI_EV_TOUCH)', 'P5=' in txt)
        check('encodeur : 7 crans', 'ENCODEUR: 7 crans' in txt)
        check('rapport complet (FIN)', '== FIN' in txt)
        print('---- rapport (virtuel : chiffres sans valeur) ----')
        print(txt)
    finally:
        subprocess.run(['mdel', '-i', img, '::/Guests/BANC.pgi'], env=env, capture_output=True)
        subprocess.run(['mdel', '-i', img, '::/Guests/boot.txt'], env=env, capture_output=True)
        subprocess.run(['mdeltree', '-i', img, '::/Guests/BANC'], env=env, capture_output=True)
    return check.done()


if __name__ == '__main__':
    sys.exit(main())
