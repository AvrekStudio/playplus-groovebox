#!/usr/bin/env python3
"""Cible playplus-1.3.0-own : un invité de la famille Felucca (SLOOP, KARMA, ORBIT…) sur Avrek OS, construit par
guest/felucca_build.py (dépôt cloné dans ~/src, commit fixé), sa matrice docs/correspondance/<nom>.json sur la carte.

  * /Guests/<NOM>.pgi listé dans le menu d'allumage, chargé au clic de l'encodeur, init finie ;
  * son écran envoyé (bitmap), la bande « MAP SD » ;
  * le pad de la première touche blanche (rangée 7, colonne 0 de la grille 16×8) fait du son ;
  * un potard (Pot2 = KNOB 1) et l'encodeur (SELECT) changent l'écran ; PLAY (B8) aussi.

usage: FEL=sloop TARGET=playplus-1.3.0-own scripts/test.sh vdev_own_felucca   ($FEL_PGI : un .pgi déjà construit)"""
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
    nom = os.environ.get('FEL', 'sloop')
    pgi = os.environ.get('FEL_PGI')
    if not pgi:
        pgi = os.path.join(wd, nom + '.pgi')
        sys.path.insert(0, os.path.join(ROOT, 'guest'))
        import felucca_build
        felucca_build.build(nom, pgi)
        NAME = felucca_build.FW[nom]['name']
    elif os.environ.get('FEL_NAME'):                      # (un invité construit ailleurs : X0X, Skiff…)
        NAME = os.environ['FEL_NAME']
    else:
        sys.path.insert(0, os.path.join(ROOT, 'guest'))
        import felucca_build
        NAME = felucca_build.FW[nom]['name']
    snd = os.environ.get('FEL_SOUND_PAD', '7,0')          # « r,c », ou « play » (une boîte à rythmes : PLAY fait du son)
    pr, pc = (7, 0) if snd == 'play' else (int(x) for x in snd.split(','))
    data = open(pgi, 'rb').read()
    snapshot_for(sys.argv[1])
    img = os.path.join(wd, 'sd.img')
    d = os.path.join(wd, 'shots')
    os.makedirs(d, exist_ok=True)
    env = dict(os.environ, MTOOLS_SKIP_CHECK='1')
    subprocess.run(['mmd', '-i', img, '::/Guests'], env=env, capture_output=True)
    subprocess.run(['mdeltree', '-i', img, '::/Guests/' + NAME], env=env, capture_output=True)
    try:
        sd_put(img, '/Guests/%s.pgi' % NAME, data)
        subprocess.run(['mmd', '-i', img, '::/Guests/' + NAME], env=env, capture_output=True)
        sd_put(img, '/Guests/%s/map.json' % NAME, open(os.path.join(ROOT, 'docs', 'correspondance', nom + '.json'), 'rb').read())
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
        check('menu : %s listé' % NAME, m and cstr(vp, m + 8) == NAME + '.pgi', cstr(vp, m + 8) if m else '-')
        s.tap(6, after_ms=100)
        for _ in range(80):
            if r('pgi') and r('ready'):
                break
            s.run_ms(200)
        base = r('pgi')                                    # (0x81E00000, ou la grande zone 0x81800000 de 0.6.0)
        check('%s chargé en SDRAM, init finie' % NAME, base in (SDRAM_BASE, 0x81800000) and r('ready') == 1
              and cstr(vp, base + 16, 16) == NAME, '%#x %s msg=%s' % (
                  base, cstr(vp, base + 16, 16) if base else '-', cstr(vp, r('msg')) if r('msg') else '-'))
        vp.eve.cmd_log.clear()
        vp.eve.max_log = 200000
        s.run_ms(1500)
        check('son image envoyée (bitmap)', r('bitmap') == 1)
        txt = [a[2] for n, a in vp.eve.cmd_log if n == 'TEXT' and len(a) > 2]
        check('matrice lue sur la carte (bande « MAP SD »)', 'MAP' in txt and 'SD' in txt, str(sorted(set(txt)))[:300])
        s.vp.eve.screenshot(os.path.join(d, 'own_%s_1.png' % nom), 2)
        r0 = r('renders')
        s.run_ms(500)
        check('rendu audio appelé', r('renders') - r0 > 100, str(r('renders') - r0))
        vp.inputs.pad(pr, pc, 0)                          # 1re touche blanche (ou $FEL_SOUND_PAD « r,c »)
        peak = 0
        for _ in range(30):
            s.run_ms(13)
            peak = max(peak, out_peak(vp))
        vp.inputs.pad(pr, pc, 1)
        s.run_ms(400)
        if snd != 'play':
            check('pad r%d c%d (une touche du FM-1) : du son' % (pr, pc), peak > 300, str(peak))
        s.vp.eve.screenshot(os.path.join(d, 'own_%s_2_note.png' % nom), 2)
        if os.environ.get('FEL_PROBE'):                    # (diagnostic : variables de l'invité, adresses de la carte .elf)
            pr_ = dict(x.split('=') for x in os.environ['FEL_PROBE'].split(','))
            def probe(tag):
                print('PROBE', tag, {k: hex(vp.r32(int(v, 16))) for k, v in pr_.items()})
            probe('avant')
            vp.inputs.button(8, 0); s.run_ms(30); probe('B8 tenu'); vp.inputs.button(8, 1); s.run_ms(300); probe('B8 lâché')
        f0 = texts_frame(vp, s)
        s.knob(1, 4, after_ms=500)                        # Pot2 = KNOB 1
        check('Pot2 (KNOB 1) tourné : l’écran change', texts_frame(vp, s) != f0)
        s.vp.eve.screenshot(os.path.join(d, 'own_%s_3_k1.png' % nom), 2)
        f0 = texts_frame(vp, s)
        s.turn(2, after_ms=500)                           # encodeur = SELECT
        check('encodeur (SELECT) tourné : l’écran change', texts_frame(vp, s) != f0)
        s.vp.eve.screenshot(os.path.join(d, 'own_%s_4_select.png' % nom), 2)
        f0 = texts_frame(vp, s)
        s.tap(8, after_ms=100)                            # PLAY
        peak = 0
        for _ in range(60):
            s.run_ms(20)
            peak = max(peak, out_peak(vp))
        check('PLAY (B8) : l’écran change', texts_frame(vp, s) != f0)
        if snd == 'play':
            check('PLAY : du son (la séquence)', peak > 300, str(peak))
        s.vp.eve.screenshot(os.path.join(d, 'own_%s_5_play.png' % nom), 2)
        s.tap(8, after_ms=600)
    finally:
        subprocess.run(['mdel', '-i', img, '::/Guests/%s.pgi' % NAME], env=env, capture_output=True)
        subprocess.run(['mdel', '-i', img, '::/Guests/boot.txt'], env=env, capture_output=True)
        subprocess.run(['mdeltree', '-i', img, '::/Guests/' + NAME], env=env, capture_output=True)
    return check.done()


def texts_frame(vp, s):
    """empreinte de l'image affichée (le rendu de l'écran virtuel)"""
    import hashlib
    return hashlib.md5(vp.eve.renderer.image(1).tobytes()).hexdigest()


if __name__ == '__main__':
    sys.exit(main())
