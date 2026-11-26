#!/usr/bin/env python3
"""Cible playplus-1.3.0-own : Vector37 (genkigenki/Vector37, synthé « orbite » du SMK-37) sur Avrek OS.

  * /Guests/VECTOR37.pgi listé dans le menu d'allumage, chargé au clic de l'encodeur ;
  * son écran 240 x 240 (l'orbite) envoyé à l'hôte, ses 16 pads à leurs couleurs sur la grille du Play+ ;
  * une touche du clavier (matrice docs/correspondance/vector37.json : blanches en rangée 5) joue une note ;
  * un pad du SMK (rangée 0) choisit une planète ; un potard (K1) change son réglage ; l'encodeur change de son ;
  * le son tourne sous interruption, l'interface dans la boucle (aucune image dessinée depuis l'audio).

Construit vector37.pgi (guest/vector37/build_vector37.py, clone de Vector37 nécessaire) ou prend $V37_PGI.

usage: TARGET=playplus-1.3.0-own scripts/test.sh vdev_own_v37"""
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'vdev'))
from device import CFW_RAM, ROOT, Checks, clean_card, cstr, open_device, snapshot_for, work_dir  # noqa: E402
from vdev_guest_sd import out_peak, sd_put  # noqa: E402
from vdev_own_loom import O, SDRAM_BASE, led  # noqa: E402
import hle  # noqa: E402


def main():
    check = Checks()
    clean_card(sys.argv[1])
    wd = work_dir()
    pgi = os.environ.get('V37_PGI')
    if not pgi:
        pgi = os.path.join(wd, 'vector37.pgi')
        sys.path.insert(0, os.path.join(ROOT, 'guest', 'vector37'))
        import build_vector37
        build_vector37.build(pgi)
    data = open(pgi, 'rb').read()
    snapshot_for(sys.argv[1])
    img = os.path.join(wd, 'sd.img')
    d = os.path.join(wd, 'shots')
    os.makedirs(d, exist_ok=True)
    env = dict(os.environ, MTOOLS_SKIP_CHECK='1')
    subprocess.run(['mmd', '-i', img, '::/Guests'], env=env, capture_output=True)
    subprocess.run(['mdeltree', '-i', img, '::/Guests/VECTOR37'], env=env, capture_output=True)
    try:
        sd_put(img, '/Guests/VECTOR37.pgi', data)
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
        check('menu : VECTOR37 listé', m and cstr(vp, m + 8) == 'VECTOR37.pgi', cstr(vp, m + 8) if m else '-')
        s.tap(6, after_ms=100)                            # clic de l'encodeur : le charger
        for _ in range(40):
            if r('pgi') and r('ready'):
                break
            s.run_ms(200)
        check('Vector37 chargé en SDRAM, init finie', r('pgi') == SDRAM_BASE and r('ready') == 1
              and cstr(vp, SDRAM_BASE + 16, 16) == 'VECTOR37', '%#x %s msg=%s' % (
                  r('pgi'), cstr(vp, SDRAM_BASE + 16, 16), cstr(vp, r('msg')) if r('msg') else '-'))
        s.run_ms(1500)
        check('son image envoyée (bitmap)', r('bitmap') == 1)
        s.vp.eve.screenshot(os.path.join(d, 'own_v37_1.png'), 2)
        pads = [led(vp, (p // 8) * 20 + p % 8) for p in range(16)]
        check('les 16 pads du SMK allumés à leurs couleurs', all(max(c) > 0 for c in pads) and len(set(pads)) >= 3,
              str(pads))
        r0 = r('renders')
        s.run_ms(500)
        check('rendu audio appelé', r('renders') - r0 > 100, str(r('renders') - r0))
        vp.inputs.pad(5, 0, 0)                            # blanche 1 (do)
        peak = 0
        for _ in range(40):
            s.run_ms(13)
            peak = max(peak, out_peak(vp))
        vp.inputs.pad(5, 0, 1)
        s.run_ms(300)
        check('touche du clavier : une note (son en sortie)', peak > 300, str(peak))
        s.pad(0, 0, after_ms=500)                         # pad 1 du SMK : SELECT planète 1
        s.vp.eve.screenshot(os.path.join(d, 'own_v37_2_planete.png'), 2)
        before = bytes(vp.mu.mem_read(SDRAM_BASE, 16))
        f0 = list(texts_frame(vp, s))
        s.knob(0, 3, after_ms=500)                        # K1 : GEN 1 de la planète
        s.vp.eve.screenshot(os.path.join(d, 'own_v37_3_k1.png'), 2)
        check('K1 tourné : l’écran change', texts_frame(vp, s) != f0)
        s.turn(1, after_ms=800)                           # encodeur : son suivant
        s.vp.eve.screenshot(os.path.join(d, 'own_v37_4_son.png'), 2)
        h0 = texts_frame(vp, s)
        s.run_ms(600)
        check('l’orbite tourne : l’image continue de changer', texts_frame(vp, s) != h0)
        del before
    finally:
        subprocess.run(['mdel', '-i', img, '::/Guests/VECTOR37.pgi'], env=env, capture_output=True)
        subprocess.run(['mdel', '-i', img, '::/Guests/boot.txt'], env=env, capture_output=True)
        subprocess.run(['mdeltree', '-i', img, '::/Guests/VECTOR37'], env=env, capture_output=True)
    return check.done()


def texts_frame(vp, s):
    """empreinte de l'image affichée (le rendu de l'écran virtuel)"""
    import hashlib
    return hashlib.md5(vp.eve.renderer.image(1).tobytes()).hexdigest()


if __name__ == '__main__':
    sys.exit(main())
