#!/usr/bin/env python3
"""Cible playplus-1.3.0-own : FM1 Tonic (luigirossi-music/fm1-tonic, boîte à rythmes du FM-1) sur Avrek OS.

  * /Guests/TONIC.pgi listé dans le menu d'allumage, chargé au clic de l'encodeur ; la matrice lue sur la carte
    (/Guests/TONIC/map.json = docs/correspondance/tonic.json, bande « MAP SD ») ;
  * son écran envoyé, les pads de la matrice allumés (steps en rangée 7, pistes en rangée 6) ;
  * TRACK 1 (r6 c0) joue la voix 1 ; deux steps posés (r7 c0, c4) s'allument ; PLAY (B8) lance la séquence :
    du son, la tête de lecture bouge ; PLAY l'arrête ;
  * SAVE : les objets de Tonic écrits en fichiers dans /Guests/TONIC/.

Construit tonic.pgi (guest/tonic/build_tonic.py, clone de Tonic nécessaire) ou prend $TONIC_PGI.

usage: TARGET=playplus-1.3.0-own scripts/test.sh vdev_own_tonic"""
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
    pgi = os.environ.get('TONIC_PGI')
    if not pgi:
        pgi = os.path.join(wd, 'tonic.pgi')
        sys.path.insert(0, os.path.join(ROOT, 'guest', 'tonic'))
        import build_tonic
        build_tonic.build(pgi)
    data = open(pgi, 'rb').read()
    snapshot_for(sys.argv[1])
    img = os.path.join(wd, 'sd.img')
    d = os.path.join(wd, 'shots')
    os.makedirs(d, exist_ok=True)
    env = dict(os.environ, MTOOLS_SKIP_CHECK='1')
    subprocess.run(['mmd', '-i', img, '::/Guests'], env=env, capture_output=True)
    subprocess.run(['mdeltree', '-i', img, '::/Guests/VECTOR37'], env=env, capture_output=True)
    subprocess.run(['mmd', '-i', img, '::/Guests'], env=env, capture_output=True)
    subprocess.run(['mdeltree', '-i', img, '::/Guests/TONIC'], env=env, capture_output=True)
    try:
        sd_put(img, '/Guests/TONIC.pgi', data)
        subprocess.run(['mmd', '-i', img, '::/Guests/TONIC'], env=env, capture_output=True)
        sd_put(img, '/Guests/TONIC/map.json', open(os.path.join(ROOT, 'docs', 'correspondance', 'tonic.json'), 'rb').read())
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
        check('menu : TONIC listé', m and cstr(vp, m + 8) == 'TONIC.pgi', cstr(vp, m + 8) if m else '-')
        s.tap(6, after_ms=100)
        for _ in range(60):
            if r('pgi') and r('ready'):
                break
            s.run_ms(200)
        check('Tonic chargé en SDRAM, init finie', r('pgi') == SDRAM_BASE and r('ready') == 1
              and cstr(vp, SDRAM_BASE + 16, 16) == 'TONIC', '%#x %s msg=%s' % (
                  r('pgi'), cstr(vp, SDRAM_BASE + 16, 16), cstr(vp, r('msg')) if r('msg') else '-'))
        vp.eve.cmd_log.clear()
        vp.eve.max_log = 200000
        s.run_ms(1500)
        check('son image envoyée (bitmap)', r('bitmap') == 1)
        check('sans /Guests/dma.on : envoi bloquant, aucun eDMA (0.5.2)', r('dma_off') == 1 and getattr(vp, 'edma_transfers', 0) == 0,
              '%d %d' % (r('dma_off'), getattr(vp, 'edma_transfers', 0)))
        txt = [a[2] for n, a in vp.eve.cmd_log if n == 'TEXT' and len(a) > 2]
        check('matrice lue sur la carte (bande « MAP SD »)', 'MAP' in txt and 'SD' in txt, str(sorted(set(txt)))[:300])
        s.vp.eve.screenshot(os.path.join(d, 'own_tonic_1.png'), 2)
        steps = [led(vp, 140 + c) for c in range(16)]
        trk = [led(vp, 120 + c) for c in range(8)]
        check('pads des steps (r7) et des pistes (r6) allumés', all(max(c) > 0 for c in steps + trk), str(steps[:4] + trk[:2]))
        r0 = r('renders')
        s.run_ms(500)
        check('rendu audio appelé', r('renders') - r0 > 100, str(r('renders') - r0))
        vp.inputs.pad(6, 0, 0)                            # TRACK 1 : la voix 1 joue
        peak = 0
        for _ in range(30):
            s.run_ms(13)
            peak = max(peak, out_peak(vp))
        vp.inputs.pad(6, 0, 1)
        s.run_ms(400)
        check('TRACK 1 (r6 c0) : la voix 1 sonne', peak > 300, str(peak))
        before = led(vp, 141)
        s.pad(7, 1, after_ms=300)                         # step 2 de la piste 1 (vide dans le projet de départ)
        check('step 2 posé : son pad plus vif', sum(led(vp, 141)) > sum(before), '%s -> %s' % (before, led(vp, 141)))
        s.vp.eve.screenshot(os.path.join(d, 'own_tonic_2_steps.png'), 2)
        s.tap(8, after_ms=100)                            # PLAY
        peak, seen = 0, set()
        for _ in range(80):
            s.run_ms(25)
            peak = max(peak, out_peak(vp))
            seen.add(tuple(led(vp, 140 + c) for c in range(16)))
        check('PLAY : la séquence joue (son)', peak > 300, str(peak))
        if os.environ.get('TONIC_INSNS'):                 # (mesure : instructions de l'invité par seconde, en lecture)
            from unicorn import UC_HOOK_CODE
            cnt = [0]
            hk = vp.mu.hook_add(UC_HOOK_CODE, lambda mu, ad, sz, _: cnt.__setitem__(0, cnt[0] + 1),
                                begin=SDRAM_BASE, end=SDRAM_BASE + 0x1FE000)
            s.run_ms(500)
            vp.mu.hook_del(hk)
            print('MESURE invité : %.1f M instructions par seconde de jeu' % (cnt[0] * 2 / 1e6))
        check('PLAY : la tête de lecture bouge (LED des steps)', len(seen) > 3, str(len(seen)))
        s.vp.eve.screenshot(os.path.join(d, 'own_tonic_3_play.png'), 2)
        s.tap(8, after_ms=600)                            # PLAY : arrêt (le son peut traîner : échos)
        seen = set()
        for _ in range(40):
            s.run_ms(25)
            seen.add(tuple(led(vp, 140 + c) for c in range(16)))
        check('PLAY à nouveau : arrêt (la tête de lecture ne bouge plus)', len(seen) == 1, str(len(seen)))
        f0 = texts_frame(vp, s)
        s.knob(1, 4, after_ms=400)                        # Pot2 = KNOB 1 de la page
        check('Pot2 (KNOB 1) tourné : l’écran change', texts_frame(vp, s) != f0)
        s.turn(1, after_ms=400)                           # encodeur = SELECT : page suivante
        s.vp.eve.screenshot(os.path.join(d, 'own_tonic_4_select.png'), 2)
        s.tap(4, after_ms=800)                            # SAVE (B4) : SAVE HERE?
        s.vp.eve.screenshot(os.path.join(d, 'own_tonic_5_save.png'), 2)
        s.tap(6, after_ms=3000)                           # clic de l'encodeur (B6) = OCT+ : OUI
        s.vp.eve.screenshot(os.path.join(d, 'own_tonic_6_saved.png'), 2)
        ls = subprocess.run(['mdir', '-b', '-i', img, '::/Guests/TONIC'], env=env, capture_output=True, text=True).stdout
        names = [x.rsplit('/', 1)[-1].lower() for x in ls.split()]
        check('SAVE puis OUI : le projet écrit sur la carte (/Guests/TONIC/pNN.bin)',
              any(n.startswith('p') and n.endswith('.bin') for n in names), str(names))
    finally:
        subprocess.run(['mdel', '-i', img, '::/Guests/TONIC.pgi'], env=env, capture_output=True)
        subprocess.run(['mdel', '-i', img, '::/Guests/boot.txt'], env=env, capture_output=True)
        subprocess.run(['mdeltree', '-i', img, '::/Guests/TONIC'], env=env, capture_output=True)
    return check.done()


def texts_frame(vp, s):
    """empreinte de l'image affichée (le rendu de l'écran virtuel)"""
    import hashlib
    return hashlib.md5(vp.eve.renderer.image(1).tobytes()).hexdigest()


if __name__ == '__main__':
    sys.exit(main())
