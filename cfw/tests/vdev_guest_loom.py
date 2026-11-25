#!/usr/bin/env python3
"""LOOM-8 (charlesvestal/fm1-loom) en invité sur le Play+ virtuel : chargement depuis /Guests en SDRAM,
image 240 x 240 au centre de l'écran, son, commandes. Captures d'écran à la taille réelle de l'écran du
Play+ (320 x 240) et agrandies x 3, dans le dossier des captures.

Construit loom.pgi (guest/loom/build_loom.py : clone de LOOM-8 nécessaire, ou $LOOM_PGI déjà construit).

usage: vdev_guest_loom.py <image.ppf>"""
import os
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from device import CFW_RAM, ROOT, Checks, cstr, open_device, snapshot_for, work_dir  # noqa: E402
from vdev_guest_sd import out_peak, sd_del, sd_put  # noqa: E402

GUEST = CFW_RAM + 0x40
F_ACTIVE, F_PGI, F_PICKING, F_BASE, F_BITMAP = 1, 28, 29, 32, 33
SDRAM_BASE = 0x81E00000


def g32(vp, k):
    return vp.r32(GUEST + 4 * k)


def shots(s, d, name):
    """capture à la taille réelle (320 x 240) et agrandie x 3"""
    s.vp.eve.screenshot(os.path.join(d, name + '_reel.png'), 1)
    s.vp.eve.screenshot(os.path.join(d, name + '_x3.png'), 3)


def px(vp, x, y):
    return vp.eve.renderer.image(1).convert('RGB').getpixel((x, y))


STEP1 = (50, 86)           # écran du Play+ : bas de la case du pas 1 de LOOM-8 (bande de 40 px + x 10 de LOOM-8)
ENV = dict(os.environ, MTOOLS_SKIP_CHECK='1')


def sd_ls(img, d):
    return subprocess.run(['mdir', '-b', '-i', img, '::' + d], capture_output=True, text=True, env=ENV).stdout


def wipe_saves(img):
    """les sauvegardes de LOOM-8 d'un lancement précédent fausseraient le test (projet déjà rempli)"""
    subprocess.run(['mdeltree', '-i', img, '::/Guests/LOOM-8'], env=ENV, capture_output=True)


def enter_loom(s):
    import flows
    flows.open_menu(s)
    flows.enter_item(s, 7, after_ms=800)
    s.tap(6)
    s.run_ms(1500)


def main():
    check = Checks()
    wd = work_dir()
    pgi = os.environ.get('LOOM_PGI')
    if not pgi:
        pgi = os.path.join(wd, 'loom.pgi')
        sys.path.insert(0, os.path.join(ROOT, 'guest', 'loom'))
        import build_loom
        build_loom.build(pgi)
    data = open(pgi, 'rb').read()
    snapshot_for(sys.argv[1])
    img = os.path.join(wd, 'sd.img')
    d = os.path.join(wd, 'shots')
    os.makedirs(d, exist_ok=True)
    subprocess.run(['mmd', '-i', img, '::/Guests'], env=dict(os.environ, MTOOLS_SKIP_CHECK='1'), capture_output=True)
    try:
        wipe_saves(img)
        check('carte sans sauvegarde de LOOM-8 au départ', 'o00' not in sd_ls(img, '/Guests/LOOM-8').lower())
        sd_put(img, '/Guests/loom.pgi', data)
        import flows
        import hle
        s = open_device(sys.argv[1])
        vp = s.vp
        hle.enable_audio(vp)
        s.run_ms(200)
        flows.open_menu(s)
        flows.enter_item(s, 7, after_ms=800)
        check('sélecteur : LOOM-8 listé', g32(vp, F_PICKING) == 1)
        s.tap(6)                                  # clic de l'encodeur : charger
        s.run_ms(1500)
        check('LOOM-8 chargé en SDRAM', g32(vp, F_PGI) == 1 and g32(vp, F_BASE) == SDRAM_BASE
              and cstr(vp, SDRAM_BASE + 16, 16) == 'LOOM-8', '%#x %s' % (g32(vp, F_BASE), cstr(vp, SDRAM_BASE + 16, 16)))
        check('LOOM-8 envoie son image (écran du Play+ en mode image)', g32(vp, F_BITMAP) == 1)
        shots(s, d, 'loom_1_demarrage')
        s.run_ms(2500)
        shots(s, d, 'loom_2_accueil')
        empty = px(vp, *STEP1)
        for col in (0, 4, 8, 12):                 # touches blanches = pas 1, 5, 9, 13 : quatre grosses caisses
            s.pad(7, col)
        s.run_ms(300)
        shots(s, d, 'loom_2b_pas')
        placed = px(vp, *STEP1)
        check('pas posé visible (case du pas 1 changée)', placed != empty, '%s / %s' % (placed, empty))
        s.pad(1, 18)                              # pad de fonction -> SAVE
        s.run_ms(1500)
        shots(s, d, 'loom_2c_sauve')
        s.tap(8)                                  # Play -> PLAY de LOOM-8
        peak = 0
        for _ in range(150):                      # 2 s, crête relevée toutes les ~13 ms (le son de sortie n'en
            s.run_ms(13)                          # garde que 3 ms : une mesure unique tombe entre deux coups)
            peak = max(peak, out_peak(vp))
        check('Play : LOOM-8 joue les grosses caisses (son en sortie)', peak > 1000, str(peak))
        shots(s, d, 'loom_3_lecture')
        s.turn(3)                                 # encodeur -> SELECT
        s.run_ms(800)
        shots(s, d, 'loom_4_select')
        s.pad(2, 16)                              # pad de fonction -> SEQ
        s.run_ms(800)
        shots(s, d, 'loom_5_seq')
        s.pad(1, 16)                              # pad de fonction -> GLO
        s.run_ms(800)
        shots(s, d, 'loom_6_glo')
        vp.inputs.button(10, 0)
        s.run_ms(60)
        s.tap(7)
        vp.inputs.button(10, 1)
        s.run_ms(300)
        check('Shift + Back : sortie, Polyend reprend', g32(vp, F_ACTIVE) == 0)
        del s, vp

        # sauvegarde : fichiers sur la carte, puis rechargement après un redémarrage complet
        ls = sd_ls(img, '/Guests/LOOM-8')
        files = [x for x in ls.split() if x.lower().endswith('.bin')]
        check('SAVE : objets écrits dans /Guests/LOOM-8', len(files) >= 2, ls)
        s = open_device(sys.argv[1])
        vp = s.vp
        hle.enable_audio(vp)
        s.run_ms(200)
        enter_loom(s)
        s.run_ms(1500)
        shots(s, d, 'loom_7_recharge')
        check('après redémarrage : le projet sauvé revient (pas 1 posé)', px(vp, *STEP1) == placed,
              '%s / attendu %s' % (px(vp, *STEP1), placed))
    finally:
        sd_del(img, '/Guests/loom.pgi')
        wipe_saves(img)
    return check.done()


if __name__ == '__main__':
    sys.exit(main())
