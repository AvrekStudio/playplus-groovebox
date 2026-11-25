#!/usr/bin/env python3
"""Mode invité : sélecteur d'invités (guest_pick.c), plusieurs fichiers PGI dans /Guests.

  * Menu > Mode invite liste les *.pgi de /Guests (un autre fichier est ignoré), un pad bleu par invité,
  * un fichier abîmé choisi au pad est refusé (« Fichier refuse »), on reste dans le sélecteur,
  * encodeur + clic charge « demo », qui joue ; Shift + clic de l'encodeur revient au sélecteur (silence),
  * un pad charge « bip », qui joue à son tour : changement d'invité à la volée,
  * Shift + Back : sortie.

usage: vdev_guest_pick.py <image.ppf>"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from device import CFW_RAM, ROOT, Checks, cstr, open_device, snapshot_for, work_dir  # noqa: E402
from vdev_guest_sd import PGI_BASE, led, out_peak, sd_del, sd_put  # noqa: E402

GUEST = CFW_RAM + 0x40
F_ACTIVE, F_PGI, F_PICKING, F_PICK, F_ERR = 1, 28, 29, 30, 31


def g32(vp, k):
    return vp.r32(GUEST + 4 * k)


def names(vp):
    p = g32(vp, F_PICK)
    n = vp.r32(p)
    return [cstr(vp, p + 8 + 28 * i, 28) for i in range(n)]


def main():
    check = Checks()
    sys.path.insert(0, os.path.join(ROOT, 'guest'))
    import build as gbuild
    wd = work_dir()
    demo = open(gbuild.build(os.path.join(wd, 'demo.pgi'), [os.path.join(ROOT, 'guest', 'demo', 'demo.c')]), 'rb').read()
    bip = open(gbuild.build(os.path.join(wd, 'bip.pgi'), [os.path.join(ROOT, 'guest', 'bip', 'bip.c')]), 'rb').read()
    snapshot_for(sys.argv[1])
    img = os.path.join(wd, 'sd.img')
    files = {'/Guests/demo.pgi': demo, '/Guests/bip.pgi': bip, '/Guests/bad.pgi': b'XXXX' + demo[4:],
             '/Guests/lisezmoi.txt': b'pas un invite'}
    shots = os.path.join(wd, 'shots')
    os.makedirs(shots, exist_ok=True)
    try:
        import subprocess
        subprocess.run(['mmd', '-i', img, '::/Guests'], env=dict(os.environ, MTOOLS_SKIP_CHECK='1'),
                       capture_output=True)
        for p, d in files.items():
            sd_put(img, p, d)
        import flows
        import hle
        s = open_device(sys.argv[1])
        vp = s.vp
        hle.enable_audio(vp)
        s.run_ms(200)
        flows.open_menu(s)
        flows.enter_item(s, 7, after_ms=800)

        # 1. la liste
        nm = names(vp) if g32(vp, F_PICK) else []
        check('sélecteur affiché, aucun invité chargé', g32(vp, F_PICKING) == 1 and g32(vp, F_PGI) == 0,
              'picking %d pgi %d' % (g32(vp, F_PICKING), g32(vp, F_PGI)))
        check('les 3 .pgi de /Guests sont listés, le .txt ignoré', sorted(nm) == ['bad.pgi', 'bip.pgi', 'demo.pgi'], str(nm))
        if sorted(nm) != ['bad.pgi', 'bip.pgi', 'demo.pgi']:
            return check.done()
        i_demo, i_bip, i_bad = nm.index('demo.pgi'), nm.index('bip.pgi'), nm.index('bad.pgi')
        s.run_ms(100)
        check('LED : un pad par invité, curseur en blanc',
              led(vp, 0) == (255, 255, 255) and led(vp, 1) == (0, 60, 255) and led(vp, 2) == (0, 60, 255)
              and led(vp, 3) == (0, 0, 0), str([led(vp, i) for i in range(4)]))
        check('silence dans le sélecteur', out_peak(vp) == 0, str(out_peak(vp)))
        s.shot(os.path.join(shots, 'guest_pick.png'))

        # 2. fichier abîmé, choisi au pad
        r, c = divmod(i_bad, 20)
        s.pad(r, c)
        check('invité abîmé refusé, on reste dans le sélecteur',
              g32(vp, F_ERR) == 1 and g32(vp, F_PICKING) == 1 and g32(vp, F_PGI) == 0)

        # 3. encodeur + clic : demo
        s.turn(i_demo - vp.r32(g32(vp, F_PICK) + 4))
        s.tap(6)
        check('encodeur + clic charge « demo »', g32(vp, F_PGI) == 1 and g32(vp, F_PICKING) == 0
              and cstr(vp, PGI_BASE + 16, 16) == 'demo', cstr(vp, PGI_BASE + 16, 16))
        vp.inputs.pad(7, 0, 0)
        s.run_ms(120)
        check('« demo » joue', out_peak(vp) > 1000, str(out_peak(vp)))
        vp.inputs.pad(7, 0, 1)
        s.run_ms(100)

        # 4. retour au sélecteur : Shift + clic de l'encodeur
        vp.inputs.button(10, 0)
        s.run_ms(60)
        s.tap(6)
        vp.inputs.button(10, 1)
        s.run_ms(200)
        check('Shift + clic : retour au sélecteur, invité déchargé', g32(vp, F_PICKING) == 1 and g32(vp, F_PGI) == 0)
        check('silence au retour dans le sélecteur', out_peak(vp) == 0, str(out_peak(vp)))

        # 5. un pad : bip
        r, c = divmod(i_bip, 20)
        s.pad(r, c)
        check('un pad charge « bip » à la volée', g32(vp, F_PGI) == 1 and cstr(vp, PGI_BASE + 16, 16) == 'bip',
              cstr(vp, PGI_BASE + 16, 16))
        vp.inputs.pad(3, 10, 0)
        s.run_ms(40)
        check('« bip » joue', out_peak(vp) > 1000, str(out_peak(vp)))
        check('« bip » allume le pad joué en blanc', led(vp, 70) == (255, 255, 255), str(led(vp, 70)))
        vp.inputs.pad(3, 10, 1)

        # 6. sortie
        vp.inputs.button(10, 0)
        s.run_ms(60)
        s.tap(7)
        vp.inputs.button(10, 1)
        s.run_ms(200)
        check('Shift + Back : sortie du mode invité', g32(vp, F_ACTIVE) == 0)
    finally:
        for p in files:
            sd_del(img, p)
    return check.done()


if __name__ == '__main__':
    sys.exit(main())
