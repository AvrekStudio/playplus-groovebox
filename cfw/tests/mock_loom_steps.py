#!/usr/bin/env python3
"""Maquette de la page Steps à 12 potards (28/11, pas un test) : charge un .pgi donné (LOOM_PGI) et fait des
captures dans shots/mock_*.png : piste synthé, la même en sample, kit (son synthé), kit (son sample).

usage: LOOM_PGI=... TARGET=playplus-1.3.0-own scripts/test.sh mock_loom_steps"""
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'vdev'))
from device import CFW_RAM, Checks, clean_card, open_device, snapshot_for, work_dir  # noqa: E402
from vdev_guest_sd import sd_put  # noqa: E402


def main():
    check = Checks()
    clean_card(sys.argv[1])
    wd = work_dir()
    data = open(os.environ['LOOM_PGI'], 'rb').read()
    snapshot_for(sys.argv[1])
    img = os.path.join(wd, 'sd.img')
    d = os.path.join(wd, 'shots')
    os.makedirs(d, exist_ok=True)
    env = dict(os.environ, MTOOLS_SKIP_CHECK='1')
    subprocess.run(['mmd', '-i', img, '::/Guests'], env=env, capture_output=True)
    subprocess.run(['mdeltree', '-i', img, '::/Guests/LOOM-8'], env=env, capture_output=True)
    sd_put(img, '/Guests/LOOM-8.pgi', data)
    sd_put(img, '/Guests/dma.on', b'')
    s = open_device(sys.argv[1])
    vp = s.vp
    for _ in range(60):
        st = vp.r32(CFW_RAM)
        m = st and vp.r32(st + 4) == 1 and vp.r32(st + 300)
        if m and vp.r32(m) == 1:
            break
        s.run_ms(200)
    s.run_ms(800)
    s.tap(6, after_ms=300)                                # LOOM-8
    for _ in range(40):
        if vp.r32(st + 264) and vp.r32(st + 308):
            break
        s.run_ms(200)
    s.run_ms(2500)
    s.pad(2, 16, after_ms=300)                            # T3 (synthé)
    s.tap(9, after_ms=400)                                # Steps
    for kb, step in (((7, 0), 2), ((7, 2), 5), ((7, 4), 9), ((7, 0), 14), ((7, 7), 20), ((7, 4), 28)):
        s.pad(*kb, after_ms=120)
        s.pad(step // 16, step % 16, after_ms=150)
    vp.inputs.pad(0, 18, 0)                               # début : pas 3
    s.run_ms(120)
    s.pad(0, 2, after_ms=120)
    vp.inputs.pad(0, 18, 1)
    s.run_ms(120)
    vp.inputs.pad(0, 19, 0)                               # fin : pas 32
    s.run_ms(120)
    s.pad(1, 15, after_ms=120)
    vp.inputs.pad(0, 19, 1)
    s.run_ms(2600)                                        # (le message s'efface)
    s.vp.eve.screenshot(os.path.join(d, 'mock_synth.png'), 2)
    vp.inputs.touch(6, 0)                                 # PITCH (OCTAVE) touché : sa case grandit
    s.run_ms(250)
    s.vp.eve.screenshot(os.path.join(d, 'mock_zoom.png'), 2)
    s.knob(6, 1, after_ms=200)
    s.vp.eve.screenshot(os.path.join(d, 'mock_zoom2.png'), 2)
    vp.inputs.touch(6, 1)
    s.run_ms(60)
    s.vp.eve.screenshot(os.path.join(d, 'mock_zoom3.png'), 2)
    s.run_ms(600)
    s.knob(7, 1, after_ms=600)                            # ENGINE : sample
    s.run_ms(2600)
    s.vp.eve.screenshot(os.path.join(d, 'mock_sample.png'), 2)
    s.knob(7, -1, after_ms=600)
    s.pad(0, 16, after_ms=300)                            # T1 (kit), son 2 : sa séquence 5 à 12
    s.tap(9, after_ms=300)
    s.pad(7, 1, after_ms=200)
    vp.inputs.pad(0, 18, 0)
    s.run_ms(120)
    s.pad(0, 4, after_ms=120)
    vp.inputs.pad(0, 18, 1)
    s.run_ms(120)
    vp.inputs.pad(0, 19, 0)
    s.run_ms(120)
    s.pad(0, 11, after_ms=120)
    vp.inputs.pad(0, 19, 1)
    s.run_ms(2600)
    s.vp.eve.screenshot(os.path.join(d, 'mock_kit.png'), 2)
    s.tap(7, after_ms=300)                                # PLAY + REC : les touches du bas
    s.tap(8, after_ms=400)
    s.vp.eve.screenshot(os.path.join(d, 'mock_kit_rec.png'), 2)
    s.tap(8, after_ms=200)
    s.tap(7, after_ms=200)
    check('captures faites', os.path.exists(os.path.join(d, 'mock_kit.png')))
    return check.done()


if __name__ == '__main__':
    sys.exit(main())
