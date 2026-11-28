#!/usr/bin/env python3
"""Cible playplus-1.3.0-own : LOOM-8 tourne sur Avrek OS (own.c + own_guest.c), sans le Play+ d'origine.

  * menu d'allumage : vide au départ (carte nettoyée), il revoit la carte toutes les 2 s et liste
    LOOM-8 ; clic de l'encodeur : chargé, choix noté dans /Guests/boot.txt ;
  * LOOM-8 envoie son image, les bandes latérales s'affichent ; grille éclatée : sur Steps le clavier
    (rangées 4-5) joue des notes, les pads des rangées 0 à 3 posent et retirent les trigs (rangée = mesure),
    LED des pas et tête de lecture ; B7 (PLAY) joue ;
  * l'image part par tranches depuis la boucle (og_flush), seules les rangées changées ; le dernier
    bouton reçu est affiché en bas à droite ;
  * interrupteur (bouton 12) : jamais transmis à LOOM-8 ; tenu 1,5 s → fw_power_off (l'extinction de
    l'officiel) ; relâché avant → rien ;
  * si l'alimentation n'est pas coupée (toujours le cas au virtuel) : veille muette, et le prochain appui
    de l'interrupteur redémarre Avrek OS ;
  * Shift + clic encodeur (B10 + B6) tenus 2 s : retour au menu SANS redémarrer (0.6.1), puis un autre
    chargement ; samples du projet sur la zone de l'invité : chargé quand même (0.6.1), et « Play+ d'origine »
    passe alors par « ! » dans boot.txt et un redémarrage.

Construit loom.pgi (guest/loom/build_loom.py, clone de LOOM-8 nécessaire) ou prend $LOOM_PGI.

usage: TARGET=playplus-1.3.0-own scripts/test.sh vdev_own_loom"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'vdev'))
from device import CFW_RAM, ROOT, Checks, clean_card, cstr, open_device, snapshot_for, work_dir  # noqa: E402
from vdev_guest_sd import out_peak, sd_put  # noqa: E402
import hle  # noqa: E402

SDRAM_BASE = 0x81E00000
POWER_OFF = 0x600aaa0c             # fw_power_off (symbols.ld)
AIRCR = 0xE000ED0C
# struct own (own.h) : champs de l'invité après les 4 voix (v = 184, 20 octets chacune)
O = dict(magic=0, active=4, buttons=100, renders=160, pgi=264, bitmap=268, msg=296, ready=308, off=312, menu=300, fb=328, fb_swap=332, pend=340, dma_busy=1340, dma_off=1352,
         menu_mem=1372, trashed=1376, exit_lock=1380)


def led(vp, i):
    """(r, g, b) de la LED du pad i, lue dans la trame encodée du pilote (tools/viewer/viewer.py)"""
    fr = bytes(vp.mu.mem_read(vp.r32(0x20004d84 + 4) + 48 * i, 48))

    def comp(o):
        return sum(1 << (7 - k) for k in range(8) if fr[o + 1 + k] == 0x07)
    return comp(16), comp(0), comp(32)


def app_render_addr(name='app_render'):
    """adresse d'un symbole (app_render par défaut) dans guest/loom/build/loom.elf (llvm-nm), None si absent"""
    import subprocess
    elf = os.path.join(ROOT, 'guest', 'loom', 'build', 'loom.elf')
    if not os.path.exists(elf):
        return None
    out = subprocess.run(['llvm-nm', elf], capture_output=True, text=True).stdout
    for line in out.splitlines():
        p = line.split()
        if len(p) == 3 and p[2] == name:
            return int(p[0], 16)
    return None


def texts(vp):
    return [a[2] for n, a in vp.eve.cmd_log if n == 'TEXT' and len(a) > 2]


def main():
    check = Checks()
    clean_card(sys.argv[1])
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
    import subprocess
    env = dict(os.environ, MTOOLS_SKIP_CHECK='1')
    subprocess.run(['mmd', '-i', img, '::/Guests'], env=env, capture_output=True)
    subprocess.run(['mdeltree', '-i', img, '::/Guests/LOOM-8'], env=env, capture_output=True)
    try:
        sd_put(img, '/Guests/LOOM-8.pgi', data)
        sd_put(img, '/Guests/dma.on', b'')                # (0.5.2 : l'eDMA seulement sur demande)
        # la matrice sur la carte (/Guests/LOOM-8/map.json) : celle du dépôt + une couleur de pad (r7 c10 rouge).
        # Tout le reste du test passe par elle (les tables compilées sont remplacées à l'init).
        import json
        mp = json.load(open(os.path.join(ROOT, 'docs', 'correspondance', 'loom-8.json'), encoding='utf-8'))
        mp['pad_colors'] = [{'pad_index': 157, 'pad_row': 7, 'pad_col': 17, 'color': '#ff0000'}]
        subprocess.run(['mmd', '-i', img, '::/Guests/LOOM-8'], env=env, capture_output=True)
        sd_put(img, '/Guests/LOOM-8/map.json', json.dumps(mp, ensure_ascii=False, indent=1).encode())
        s = open_device(sys.argv[1])
        vp = s.vp
        hle.enable_audio(vp)
        offs = [0]
        vp.hook_addr(POWER_OFF, lambda vp: offs.__setitem__(0, offs[0] + 1))
        hp = []
        from unicorn.arm_const import UC_ARM_REG_R1, UC_ARM_REG_R2
        vp.hook_addr(0x600aa09c, lambda vp: hp.append((vp.mu.reg_read(UC_ARM_REG_R1), vp.mu.reg_read(UC_ARM_REG_R2))))
        resets = [0]

        def mmio_w(mu, access, addr, size, value, _):
            if addr == AIRCR and (value & 0xFFFF0004) == 0x05FA0004:
                resets[0] += 1
        from unicorn import UC_HOOK_MEM_WRITE
        vp.mu.hook_add(UC_HOOK_MEM_WRITE, mmio_w, begin=AIRCR, end=AIRCR + 3)

        st = 0
        for _ in range(60):                               # prise de main, puis le menu revoit la carte (2 s)
            st = vp.r32(CFW_RAM)
            m = st and vp.r32(st + 4) == 1 and vp.r32(st + O['menu'])
            if m and vp.r32(m) == 1:
                break
            s.run_ms(200)
        r = lambda k: vp.r32(st + O[k])                   # noqa: E731
        check('la main est prise', st and r('magic') == 0x4F574E31 and r('active') == 1, hex(st))
        m = r('menu')
        check('menu d’allumage : LOOM-8 listé', m and vp.r32(m) == 1 and cstr(vp, m + 8) == 'LOOM-8.pgi',
              '%#x %s' % (m, cstr(vp, m + 8) if m else '-'))
        vp.eve.max_log = 200000
        vp.eve.cmd_log.clear()
        s.run_ms(1200)
        s.vp.eve.screenshot(os.path.join(d, 'own_menu.png'), 2)
        check('menu à l’écran (LOOM-8, Boucle de test, Play+ d’origine)',
              all(x in texts(vp) for x in ('AVREK OS', 'LOOM-8', 'Boucle de test')), str(sorted(set(texts(vp))))[:200])
        vp.inputs.button(6, 0)                            # clic de l'encodeur : curseur sur LOOM-8
        s.run_ms(100)
        vp.inputs.button(6, 1)
        for _ in range(30):
            if r('pgi') and r('ready'):
                break
            s.run_ms(200)
        boot = subprocess.run(['mtype', '-i', img, '::/Guests/boot.txt'], env=env, capture_output=True, text=True).stdout
        check('dernier choix enregistré (/Guests/boot.txt)', boot == 'LOOM-8.pgi', repr(boot))
        check('LOOM-8 chargé en SDRAM, init finie', r('pgi') == SDRAM_BASE and r('ready') == 1
              and cstr(vp, SDRAM_BASE + 16, 16) == 'LOOM-8',
              '%#x ready=%d %s msg=%s' % (r('pgi'), r('ready'), cstr(vp, SDRAM_BASE + 16, 16),
                                         cstr(vp, r('msg')) if r('msg') else '-'))
        s.run_ms(1500)
        check('LOOM-8 envoie son image', r('bitmap') == 1)
        check('image envoyée par eDMA (en tâche de fond)', getattr(vp, 'edma_transfers', 0) > 10,
              str(getattr(vp, 'edma_transfers', 0)))
        # personne ne parle à l'écran pendant un envoi eDMA (sur la machine il dure ~2 ms par 16 rangées : un accès
        # au milieu coupe la transaction et bloque le LPSPI3 ; au virtuel le DMA est instantané mais dma_busy reste
        # mis jusqu'au tour suivant de la boucle, ce qui suffit à voir l'accès)
        bad = []
        def spy(name):
            def f(vp_):
                if vp_.r32(st + O['dma_busy']):
                    bad.append((name, hex(vp_.mu.reg_read(UC_ARM_REG_LR))))
            return f
        from unicorn.arm_const import UC_ARM_REG_LR
        h1 = vp.hook_addr(0x600a9534, spy('cs_low'))
        h2 = vp.hook_addr(0x600b8bc0, spy('lpspi_xfer'))
        t0 = getattr(vp, 'edma_transfers', 0)
        for k in range(12):                               # de quoi redessiner : potards, dans les deux sens
            s.knob(1, -1 if k % 2 else 1, after_ms=120)   # (DIR : finit sur FWD)
        check('aucun accès à l’écran pendant un envoi eDMA', not bad, str(sorted(set(bad)))[:200])
        check('… pendant que l’image change (envois eDMA)', getattr(vp, 'edma_transfers', 0) - t0 > 5,
              str(getattr(vp, 'edma_transfers', 0) - t0))
        dmat = subprocess.run(['mtype', '-i', img, '::/Guests/dma.txt'], env=env, capture_output=True, text=True).stdout
        check('premiers transferts eDMA relus dans RAM_G : justes (/Guests/dma.txt « OK »)', ': OK' in dmat, repr(dmat[:200]))
        check('volume casque du codec à +6 dB (fw_codec_hp 0x0C)', (0x0C, 0x0C) in hp, str(hp[:3]))
        vp.eve.cmd_log.clear()
        vp.eve.max_log = 200000
        s.run_ms(400)
        txt = texts(vp)
        check('bandes latérales (charge CPU)', 'CPU' in txt, str(sorted(set(txt)))[:300])
        check('matrice lue sur la carte (bande « MAP SD »)', 'MAP' in txt and 'SD' in txt, str(sorted(set(txt)))[:300])
        check('couleur de pad de map.json (r7 c17, pad libre de la matrice : rouge faible)',
              led(vp, 157)[0] > 0 and led(vp, 157)[1:] == (0, 0), str(led(vp, 157)))
        # un appel audio du Play+ (128 trames) = un rendu LOOM de 128 trames (0.6.1) : avant, un rendu de
        # 256 trames tous les deux appels, soit un bloc entier de travail dans une fenêtre de 2,9 ms
        # (craquements sur la machine, PIC 140 %). app_render repéré dans loom.elf (même construction).
        ar = app_render_addr()
        calls = []
        if ar:
            vp.hook_addr(ar, lambda vp: calls.append(vp.mu.reg_read(UC_ARM_REG_R1)))
        r0 = r('renders')
        s.run_ms(500)
        nr = r('renders') - r0
        check('son : LOOM-8 appelé par le rendu', nr > 100, str(nr))
        check('son : un rendu LOOM de 128 trames par appel audio',
              ar and set(calls) == {128} and abs(len(calls) - nr) <= 2,
              'app_render %s : %d rendus, tailles %s, %d appels' % (hex(ar or 0), len(calls), sorted(set(calls)), nr))
        # (0.6.1) B3 = SELECT + (cran d'encodeur) tenu 1,2 s : le cran se répète (guest/sdk/autorep.h : 400 ms
        # puis toutes les 70 ms). On suit rep.t (l'heure du prochain cran) : une valeur nouvelle par cran répété.
        ra, rm = app_render_addr('rep.2'), app_render_addr('rep.0')   # (clang découpe la structure : .0 m, .2 t)
        seen = set()
        vp.inputs.button(3, 0)
        for _ in range(24):
            s.run_ms(50)
            if ra:
                seen.add(vp.r32(ra))
        vp.inputs.button(3, 1)
        s.run_ms(100)
        check('B3 (SELECT +) tenu 1,2 s : cran répété (≥ 8 fois), arrêté au relâchement',
              ra and rm and len(seen) >= 9 and vp.r32(rm) == 0,
              'rep %s : %d valeurs, m=%#x' % (hex(ra or 0), len(seen), vp.r32(rm) if rm else 0))

        # grille éclatée : rangées 0 à 3 = les 64 pas (mesure r = rangée r), clavier = notes même sur Steps
        peak = 0
        vp.inputs.pad(7, 0, 0)                            # blanche 1 (C2) sur Steps : une note, pas un pas
        for _ in range(20):
            s.run_ms(13)
            peak = max(peak, out_peak(vp))
        vp.inputs.pad(7, 0, 1)
        s.run_ms(300)
        check('Steps : le clavier joue une note (son)', peak > 500, str(peak))
        check('Steps : le clavier ne pose pas de pas (pad du pas 1 sans trig)', led(vp, 0)[0] < 0x80, str(led(vp, 0)))
        for col in (0, 4, 8, 12):                         # pads du haut, rangée 0 : pas 1, 5, 9, 13
            s.pad(0, col)
        s.pad(2, 3)                                       # rangée 2 = mesure 3 : pas 36
        s.run_ms(300)
        s.vp.eve.screenshot(os.path.join(d, 'own_loom_grille.png'), 2)
        check('pads du haut : trigs posés (rouge à 50 % : pas 1, 5, 9, 13, 36)',
              all(led(vp, p) == (0x80, 0, 0) for p in (0, 4, 8, 12, 43)),
              str([led(vp, p) for p in (0, 4, 8, 12, 43)]))
        check('pas vides de la séquence : blanc faible ; trigs rouges',
              led(vp, 40)[0] == led(vp, 40)[1] == led(vp, 40)[2] and 0 < led(vp, 40)[0] < 0x40 and
              led(vp, 0) == (0x80, 0, 0), '%s %s' % (led(vp, 40), led(vp, 0)))
        s.pad(2, 3)                                       # même pad : trig retiré
        s.run_ms(300)
        check('même pad tapé : trig retiré', led(vp, 43)[0] < 0x80, str(led(vp, 43)))
        algo = [led(vp, r * 20 + 16) for r in range(8)]
        check('pads ALGO : la couleur de chaque piste (8 couleurs), la piste choisie vive',
              len(set(algo)) == 8 and max(algo[0]) >= 0xf0 and all(max(x) < 0x60 for x in algo[1:]), str(algo))
        s.tap(7)                                          # B7 = PLAY de LOOM-8
        peak, heads, keys = 0, set(), set()
        for _ in range(150):                              # 2 s, crête toutes les ~13 ms
            s.run_ms(13)
            peak = max(peak, out_peak(vp))
            heads |= {p for p in range(16) if led(vp, p) == (255, 255, 255)}
            keys.add(tuple(led(vp, 140 + i) for i in range(16)))
        check('PLAY : LOOM-8 joue les trigs (son en sortie)', peak > 1000, str(peak))
        check('tête de lecture en blanc sur les pads, qui avance', len(heads) >= 4, str(sorted(heads)))
        check('Steps : blanches (dernière rangée) fixes, sans tête de lecture', len(keys) == 1, '%d états' % len(keys))
        s.tap(7)                                          # PLAY : arrêt
        # layout du propriétaire (27/11, Atelier Play+) : piano C2 sur les rangées 4 à 7, source des pas façon OP-Z,
        # OCT au relâché, OCT tenu + pas = début / fin de la séquence, Shift + OCT = rotation. On lit les pas dans
        # pats (patterns_t : pat[NSEQT][NPATT] de tpat_t, 812 o depuis Avrek LOOM ; step_t 12 o : note[4], vel, len ; tpat_t len
        # +768, lstart +774, lend +775), T3 (synthé), pattern 1.
        sym = {}
        elf = os.path.join(ROOT, 'guest', 'loom', 'build', 'loom.elf')
        for line in subprocess.run(['llvm-nm', elf], capture_output=True, text=True).stdout.split('\n'):
            p_ = line.split()
            if len(p_) == 3:
                sym[p_[2]] = int(p_[0], 16)
        tp = sym['pats'] + (2 * 8 + 0) * 812
        note = lambda i: vp.mu.mem_read(tp + 12 * i, 1)[0]          # noqa: E731
        slen = lambda i: vp.mu.mem_read(tp + 12 * i + 5, 1)[0]      # noqa: E731
        s.pad(2, 16, after_ms=300)                        # pad ALGO 3 : T3
        s.tap(9, after_ms=300)                            # SEQ : Steps
        for kb, step in (((7, 0), 0), ((6, 1), 1), ((7, 15), 2), ((6, 0), 3), ((5, 3), 12)):
            s.pad(*kb, after_ms=150)                      # une note du clavier...
            s.pad(0, step, after_ms=200)                  # ... puis un pas vide : il la pose
        check('clavier : C2 (r7 c0) = 36, C#2 (r6 c1) = 37, D4 (r7 c15) = 62, r6 c0 et rangée 5 sans note',
              [note(i) for i in (0, 1, 2, 3, 12)] == [36, 37, 62, 62, 62], str([note(i) for i in (0, 1, 2, 3, 12)]))
        check('rangées 4 et 5 éteintes, clavier : blanches jaunes, noires rouges',
              all(led(vp, 80 + i) == (0, 0, 0) and led(vp, 100 + i) == (0, 0, 0) for i in range(16)) and
              led(vp, 141)[1] > led(vp, 141)[2] and led(vp, 121)[0] > led(vp, 121)[1],
              '%s %s %s' % (led(vp, 80), led(vp, 141), led(vp, 121)))
        s.pad(0, 19, after_ms=200)                        # OCT+ tapé : octave au relâché
        s.pad(7, 0, after_ms=150)
        s.pad(0, 4, after_ms=200)
        s.pad(0, 18, after_ms=200)                        # OCT- : retour
        s.pad(7, 0, after_ms=150)
        s.pad(0, 5, after_ms=200)
        check('OCT+ puis OCT- : octave du clavier (48, puis 36)', (note(4), note(5)) == (48, 36), str((note(4), note(5))))
        vp.inputs.pad(0, 0, 0)                            # pas 1 tenu + pas 20 (mesure 2) : longueur jusqu'au pas 20
        s.run_ms(200)
        s.pad(1, 3, after_ms=200)
        vp.inputs.pad(0, 0, 1)
        s.run_ms(300)
        check('pas tenu + pas d\'une autre mesure : longueur (20 pas = code 68), pas gardé',
              slen(0) == 68 and note(0) == 36, 'len %d note %d' % (slen(0), note(0)))
        vp.inputs.pad(0, 0, 0)                            # tenu 2 s : la source
        s.run_ms(2300)
        vp.inputs.pad(0, 0, 1)
        blinks = set()
        for _ in range(14):
            s.run_ms(50)
            blinks.add(led(vp, 0))
        check('tenu 2 s : pas gardé, et il clignote (la source)', note(0) == 36 and len(blinks) >= 2, str(blinks))
        s.pad(0, 8, after_ms=300)                         # pas vide : une copie de la source
        check('pas vide après la copie : la source (note 36, longueur 68)', (note(8), slen(8)) == (36, 68),
              str((note(8), slen(8))))
        s.pad(7, 2, after_ms=150)                         # une note jouée (E2 = 40) : nouvelle source
        s.pad(0, 9, after_ms=300)
        check('note jouée : elle remplace la source (40, longueur par défaut)', (note(9), slen(9)) == (40, 0),
              str((note(9), slen(9))))
        vp.inputs.pad(0, 9, 0)                            # pas tenu + clavier : sa note (G2 = 43)
        s.run_ms(200)
        s.pad(7, 4, after_ms=200)
        vp.inputs.pad(0, 9, 1)
        s.run_ms(300)
        check('pas tenu + clavier : la note du pas change (43), le pas reste', note(9) == 43, str(note(9)))
        vp.inputs.pad(0, 18, 0)                           # OCT- tenu + pas 3 : début de la séquence
        s.run_ms(150)
        s.pad(0, 2, after_ms=150)
        vp.inputs.pad(0, 18, 1)
        s.run_ms(150)
        vp.inputs.pad(0, 19, 0)                           # OCT+ tenu + pas 32 : fin
        s.run_ms(150)
        s.pad(1, 15, after_ms=150)
        vp.inputs.pad(0, 19, 1)
        s.run_ms(400)
        ls, le, ln = (vp.mu.mem_read(tp + o, 1)[0] for o in (774, 775, 768))
        check('OCT tenu + pas : début 3, fin 32 (pas avant le début éteints)',
              ls == 2 and (le == 32 or (le == 0 and ln == 32)) and led(vp, 0) == (0, 0, 0) and led(vp, 1) == (0, 0, 0),
              'lstart %d lend %d len %d led %s' % (ls, le, ln, led(vp, 0)))
        s.pad(7, 0, after_ms=150)
        s.pad(0, 10, after_ms=200)
        check('OCT tenu pour la séquence : l\'octave ne change pas (36)', note(10) == 36, str(note(10)))
        n9 = note(9)
        vp.inputs.button(10, 0)                           # Shift + OCT+ : rotation d'un pas vers la droite
        s.run_ms(80)
        s.pad(0, 19, after_ms=200)
        vp.inputs.button(10, 1)
        s.run_ms(200)
        check('Shift + OCT+ : rotation à droite (le pas 10 passe en 11)', note(10) == n9, '%d -> %d' % (n9, note(10)))
        s.vp.eve.screenshot(os.path.join(d, 'own_loom_layout.png'), 2)
        # (28/11) DIR (FWD, RWD, PPONG, RND) au potard 2 de Steps ; chaque son d'un kit sa séquence ; REC court / tenu.
        # seq_t (seq.h, offsets mesurés avec clang) : rec +32, t[] +112, seq_trk_t 372 o : pos +0, kpos +358 ;
        # tpat_t : dir +776, kst +777, ken +788, kdir +799
        sq = sym['seq']
        tpos = lambda t: vp.mu.mem_read(sq + 112 + 372 * t, 1)[0]               # noqa: E731
        kpos = lambda t, k: vp.mu.mem_read(sq + 112 + 372 * t + 358 + k, 1)[0]  # noqa: E731

        def track(t, ms=2400, f=tpos):
            out = []
            for _ in range(ms // 25):
                s.run_ms(25)
                v = f(t)
                if not out or out[-1] != v:
                    out.append(v)
            return out
        s.knob(1, 1, after_ms=300)                        # DIR : RWD
        dir3 = vp.mu.mem_read(tp + 776, 1)[0]
        s.tap(7, after_ms=100)                            # PLAY
        rw = track(2)
        s.tap(7, after_ms=300)
        down = sum(1 for x, y in zip(rw, rw[1:]) if y == x - 1)
        check('DIR RWD : la tête recule (début 3, fin 32)', dir3 == 1 and down >= 8 and all(2 <= x < 32 for x in rw),
              'dir %d %s' % (dir3, rw[:24]))
        s.knob(1, 1, after_ms=300)                        # PPONG
        s.tap(7, after_ms=100)
        pp = track(2, 6000)
        s.tap(7, after_ms=300)
        up = sum(1 for x, y in zip(pp, pp[1:]) if y == x + 1)
        dn = sum(1 for x, y in zip(pp, pp[1:]) if y == x - 1)
        check('DIR PPONG : aller puis retour', vp.mu.mem_read(tp + 776, 1)[0] == 2 and up >= 8 and dn >= 8,
              str(pp[:40]))
        s.knob(1, -2, after_ms=300)                       # (FWD pour la suite)
        tk = sym['pats'] + (0 * 8 + 0) * 812               # T1 (kit), pattern 1
        s.pad(0, 16, after_ms=300)
        s.tap(9, after_ms=300)
        s.pad(7, 1, after_ms=200)                         # son 2 : sa séquence, pas 5 à 8
        vp.inputs.pad(0, 18, 0)
        s.run_ms(120)
        s.pad(0, 4, after_ms=120)
        vp.inputs.pad(0, 18, 1)
        s.run_ms(120)
        vp.inputs.pad(0, 19, 0)
        s.run_ms(120)
        s.pad(0, 7, after_ms=120)
        vp.inputs.pad(0, 19, 1)
        s.run_ms(200)
        kst, ken = vp.mu.mem_read(tk + 777 + 1, 1)[0], vp.mu.mem_read(tk + 788 + 1, 1)[0]
        s.tap(7, after_ms=100)
        k2 = track(0, 2400, lambda t: kpos(t, 1))
        mp = track(0, 2400)
        s.tap(7, after_ms=300)
        check('kit : le son 2 a sa séquence (5 à 8), la piste garde la sienne',
              (kst, ken) == (5, 8) and set(k2) <= {4, 5, 6, 7} and len(set(k2)) == 4 and max(mp) > 7,
              'kst %d ken %d son2 %s piste %s' % (kst, ken, k2[:12], mp[:12]))
        rec = lambda: vp.mu.mem_read(sq + 32, 1)[0]       # noqa: E731
        s.tap(7, after_ms=300)                            # PLAY
        s.tap(8, hold_ms=60, after_ms=300)                # REC court : ON
        r1 = rec()
        s.tap(8, hold_ms=60, after_ms=300)                # REC court : OFF
        r2 = rec()
        vp.inputs.button(8, 0)                            # REC tenu : ON jusqu'au relâché
        s.run_ms(900)
        r3 = rec()
        vp.inputs.button(8, 1)
        s.run_ms(300)
        r4 = rec()
        s.tap(7, after_ms=300)
        check('REC : court = ON, court = OFF, tenu = ON jusqu\'au relâché', (r1, r2, r3, r4) == (1, 0, 1, 0),
              str((r1, r2, r3, r4)))
        s.tap(9, after_ms=300)                            # SEQ : Steps
        vp.eve.cmd_log.clear()
        s.run_ms(400)
        check('dernier bouton affiché (B9 relâché)', 'B9 ^' in texts(vp), str(sorted(set(texts(vp))))[:200])
        pend = [vp.r32(st + O['pend'] + 4 * i) for i in range(8)]
        check('image envoyée par tranches : plus de rangée en attente', not any(pend), str([hex(x) for x in pend]))
        # LOOM non modifié : accélération coupée et encodeur = ALGORITHM dans une liste SEL, par notre couche
        sym = {}
        elf = os.path.join(ROOT, 'guest', 'loom', 'build', 'loom.elf')
        for line in subprocess.run(['llvm-nm', elf], capture_output=True, text=True).stdout.split('\n'):
            p = line.split()
            if len(p) == 3:
                sym[p[2]] = int(p[0], 16)
        check('accélération des encodeurs de LOOM coupée (accel_off = 1)', vp.mu.mem_read(sym['accel_off'], 1)[0] == 1)
        lst = sym['lst']
        s.tap(7)                                          # PLAY : arrêt (les listes s'ouvrent à l'arrêt aussi)
        s.tap(5, after_ms=600)                            # B5 = SEL : liste de la page
        opened = vp.mu.mem_read(lst, 1)[0]
        for _ in range(8):                                # B3 = SELECT cran + : jusqu'à ROTATE (une valeur réglable)
            if cstr(vp, vp.r32(lst + 8 + vp.mu.mem_read(lst + 2, 1)[0] * 32), 16) == 'ROTATE':
                break
            s.tap(3, after_ms=200)
        sel0 = vp.mu.mem_read(lst + 2, 1)[0]
        val0 = cstr(vp, lst + 8 + sel0 * 32 + 4, 24)
        s.turn(1, after_ms=500)
        sel1 = vp.mu.mem_read(lst + 2, 1)[0]
        val1 = cstr(vp, lst + 8 + sel1 * 32 + 4, 24)
        s.vp.eve.screenshot(os.path.join(d, 'own_loom_sel.png'), 2)
        check('SEL : liste ouverte', opened == 1, str(opened))
        check('liste : l’encodeur ne déplace pas le curseur', sel1 == sel0, '%d -> %d' % (sel0, sel1))
        check('liste : l’encodeur change la valeur (ROTATE, comme ALGORITHM)', val1 != val0, '%r -> %r' % (val0, val1))
        check('liste : pads ALGO éteints', led(vp, 16) == (0, 0, 0) and led(vp, 36) == (0, 0, 0),
              '%s %s' % (led(vp, 16), led(vp, 36)))
        s.pad(1, 16, after_ms=300)                        # pad ALGO 2 : coupé pendant la liste
        check('liste : pad ALGO sans effet (valeur et curseur inchangés)',
              vp.mu.mem_read(lst, 1)[0] == 1 and vp.mu.mem_read(lst + 2, 1)[0] == sel1
              and cstr(vp, lst + 8 + sel1 * 32 + 4, 24) == val1)
        s.tap(5, after_ms=400)                            # SEL : fermer
        check('liste fermée : pads ALGO rallumés', led(vp, 16) != (0, 0, 0), str(led(vp, 16)))
        # kit (T1) : le séquenceur ne montre que le dernier son joué ; clavier : sons séquencés en rouge (27/11)
        s.pad(0, 16, after_ms=300)                        # pad ALGO 1 : T1 (kit)
        s.tap(9, after_ms=300)                            # SEQ : Steps
        s.pad(7, 0, after_ms=200)                         # son 1 (1re blanche)
        s.pad(0, 5, after_ms=300)                         # pas 6 (vide : 3, 7, 11, 15 ont les trigs tournés) : le son 1
        on1, k1, k6 = led(vp, 5), led(vp, 140), led(vp, 145)
        s.pad(7, 1, after_ms=300)                         # son 2 joué : le pas du son 1 n'est plus montré
        off2 = led(vp, 5)
        check('kit : pas du son joué en rouge, celui d\'un autre son en blanc faible',
              on1 == (0x80, 0, 0) and off2[0] == off2[1] == off2[2] < 0x40, '%s %s' % (on1, off2))
        check('kit : touche d\'un son séquencé en rouge, d\'un son sans pas en jaune',
              k1[0] > 0 and k1[1:] == (0, 0) and k6[1] > 0, '%s %s' % (k1, k6))
        check('kit : une rangée de 11 pads (un par son), le reste du clavier éteint',
              all(led(vp, 140 + i) != (0, 0, 0) for i in range(11)) and
              all(led(vp, 140 + i) == (0, 0, 0) for i in range(11, 16)) and
              all(led(vp, 120 + i) == (0, 0, 0) for i in range(16)),
              str([led(vp, 140 + i) for i in range(16)])[:200])
        s.pad(7, 0, after_ms=200)
        s.pad(0, 5, after_ms=300)                         # (pas 6 retiré)
        # projets (27/11) : 16 sur la carte, Shift + B2 ouvre la liste, DELETE PROJECT
        img = os.path.join(work_dir(), 'sd.img')
        env = dict(os.environ, MTOOLS_SKIP_CHECK='1')

        def obj(o):
            n = ('o%03d.bin' if o >= 100 else 'o%02d.bin') % o
            r = subprocess.run(['mtype', '-i', img, '::/Guests/LOOM-8/' + n], env=env, capture_output=True)
            return r.stdout if r.returncode == 0 else None

        def obj_named(n):
            r_ = subprocess.run(['mtype', '-i', img, '::/Guests/LOOM-8/' + n], env=env, capture_output=True)
            return r_.stdout if r_.returncode == 0 else None

        def shift_b2():
            vp.inputs.button(10, 0)
            s.run_ms(60)
            s.tap(2, after_ms=600)
            vp.inputs.button(10, 1)
            s.run_ms(200)

        def rows():
            return [cstr(vp, vp.r32(lst + 8 + i * 32), 24) for i in range(vp.mu.mem_read(lst + 1, 1)[0])]
        s.tap(4, after_ms=1500)                           # B4 = SAVE : enregistré tout de suite
        saved = [k for k in range(16) if obj(2 + 7 * k)]
        check('SAVE : le projet est sur la carte (/Guests/LOOM-8/oNN.bin)', len(saved) >= 1, str(saved))
        shift_b2()
        rw = rows()
        check('Shift + B2 : la liste des projets (PROJ 1 à PROJ 16, DELETE PROJECT)',
              vp.mu.mem_read(lst, 1)[0] == 1 and any(x.startswith('PROJ 16') for x in rw) and 'DELETE PROJECT' in rw,
              str(rw)[:300])
        s.vp.eve.screenshot(os.path.join(d, 'own_loom_projets.png'), 2)
        if saved and 'DELETE PROJECT' in rw:
            tgt = rw.index('DELETE PROJECT')
            for _ in range(70):                           # B3 = SELECT + : jusqu'à DELETE PROJECT
                if vp.mu.mem_read(lst + 2, 1)[0] == tgt:
                    break
                s.tap(3, after_ms=120)
            s.turn(saved[0], after_ms=300)                # le slot enregistré
            s.tap(5, after_ms=500)                        # SEL : « DELETE PROJ n? »
            s.tap(5, after_ms=1200)                       # SEL : oui
            check('DELETE PROJECT : les objets du projet effacés de la carte', not obj(2 + 7 * saved[0]),
                  'sel %d / %d, o%02d : %r' % (vp.mu.mem_read(lst + 2, 1)[0], tgt, 2 + 7 * saved[0], obj(2 + 7 * saved[0])))
        if vp.mu.mem_read(lst, 1)[0]:
            shift_b2()                                    # (refermée)
        # « system » (28/11) : intensité des pads, luminosité de l'écran, gardées dans /Guests/LOOM-8/sys.bin
        shift_b2()
        rw = rows()
        check('menu projets / système : « system » en bas, PAD LIGHT puis SCREEN BRIGHTNESS',
              rw[-3:] == ['system', 'PAD LIGHT', 'SCREEN BRIGHTNESS'], str(rw[-4:]))

        def goto(name):
            tgt = rw.index(name)
            for _ in range(70):
                if vp.mu.mem_read(lst + 2, 1)[0] == tgt:
                    break
                s.tap(3, after_ms=100)
        before = led(vp, 140)
        goto('PAD LIGHT')
        for _ in range(10):                               # 100 % -> 50 %, cran par cran
            s.turn(-1, after_ms=120)
        after = led(vp, 140)
        goto('SCREEN BRIGHTNESS')
        for _ in range(4):                                # 100 % -> 80 %
            s.turn(-1, after_ms=120)
        s.run_ms(300)
        duty = vp.eve.reg('PWM_DUTY') & 0xff
        sysb = obj_named('sys.bin')
        check('PAD LIGHT 50 % : les pads deux fois moins forts', before[0] > 0 and abs(after[0] - before[0] // 2) <= 2,
              '%s -> %s' % (before, after))
        check('SCREEN BRIGHTNESS 80 % : REG_PWM_DUTY = 102', duty == 102, str(duty))
        check('réglages gardés sur la carte (sys.bin : S, 50, 80)', sysb is not None and sysb[:3] == b'S\x32\x50',
              repr(sysb))
        goto('PAD LIGHT')
        for _ in range(10):                               # (100 % pour la suite)
            s.turn(1, after_ms=120)
        shift_b2()
        check('Shift + B2 encore : liste fermée', vp.mu.mem_read(lst, 1)[0] == 0)
        # Charge CPU : pas vérifiée ici. Au Play+ virtuel, CYCCNT n'avance quasiment pas pendant le rendu
        # appelé par l'interruption audio injectée, même en mode exact (≈ 76 cycles par bloc) : seule la
        # vraie machine donne la charge.
        s.vp.eve.screenshot(os.path.join(d, 'own_loom_lecture.png'), 2)

        vp.inputs.button(12, 0)                           # interrupteur, relâché avant 1,5 s
        s.run_ms(800)
        vp.inputs.button(12, 1)
        s.run_ms(300)
        check('interrupteur 0,8 s : pas d’extinction', offs[0] == 0, str(offs[0]))
        vp.inputs.button(12, 0)
        s.run_ms(1700)
        check('interrupteur tenu 1,5 s : extinction (fw_power_off)', offs[0] >= 1, str(offs[0]))
        vp.inputs.button(12, 1)
        s.run_ms(200)

        check('alimentation pas coupée (virtuel) : veille', r('off') == 1)
        r0 = r('renders')
        s.run_ms(200)
        check('veille : plus d’appel à LOOM (rendu muet)', r('renders') == r0, '%d -> %d' % (r0, r('renders')))
        vp.inputs.button(12, 0)                           # nouvel appui : redémarrage d'Avrek OS
        s.run_ms(100)
        vp.inputs.button(12, 1)
        s.run_ms(100)
        check('veille + interrupteur : redémarrage', resets[0] >= 1, str(resets[0]))
        # 2e démarrage (le virtuel ne redémarre pas pour de bon : on repart de l'instantané). Dernier choix
        # « Play+ d'origine » : le curseur doit y être ; encodeur -2 → LOOM-8, clic, puis Shift + B6.
        subprocess.run(['mdel', '-i', img, '::/Guests/boot.txt'], env=env, capture_output=True)
        subprocess.run(['mdel', '-i', img, '::/Guests/dma.on', '::/Guests/dma.txt'], env=env, capture_output=True)
        sd_put(img, '/Guests/boot.txt', b'-')
        del s, vp
        s = open_device(sys.argv[1])
        vp = s.vp
        hle.enable_audio(vp)
        resets[0] = 0
        vp.mu.hook_add(UC_HOOK_MEM_WRITE, mmio_w, begin=AIRCR, end=AIRCR + 3)
        for _ in range(60):
            st = vp.r32(CFW_RAM)
            m = st and vp.r32(st + 4) == 1 and vp.r32(st + O['menu'])
            if m and vp.r32(m) == 1:
                break
            s.run_ms(200)
        check('2e menu : curseur sur le dernier choix (Play+ d’origine)', m and vp.r32(m + 4) == 2,
              str(vp.r32(m + 4) if m else -1))
        s.turn(-2)
        check('encodeur -2 : curseur sur LOOM-8', vp.r32(m + 4) == 0, str(vp.r32(m + 4)))
        # (0.6.1) projet dont les samples arrivent sur la zone de l'invité (curseur au-delà de 0x81E00000) :
        # avant, « refuse : samples du projet » ; maintenant chargé, et noté (trashed)
        pool = 0x2001b138                                 # fw_sample_pool (symbols.ld)
        cur0 = vp.r32(pool + 8)
        vp.mu.mem_write(pool + 8, (0x81F00000).to_bytes(4, 'little'))
        s.tap(6, after_ms=100)
        for _ in range(30):
            if r('pgi') and r('ready'):
                break
            s.run_ms(200)
        check('clic : LOOM-8 chargé malgré les samples du projet (écrasés, noté)',
              r('pgi') == SDRAM_BASE and r('ready') == 1 and r('trashed') == 1,
              'pgi %#x ready %d trashed %d msg %s' % (r('pgi'), r('ready'), r('trashed'), cstr(vp, r('msg'), 32) if r('msg') else '-'))
        vp.mu.mem_write(pool + 8, cur0.to_bytes(4, 'little'))
        s.run_ms(500)
        menu0 = r('menu_mem')

        def shift_click(ms):
            vp.inputs.button(10, 0)
            vp.inputs.button(6, 0)
            s.run_ms(ms)
            vp.inputs.button(6, 1)
            vp.inputs.button(10, 1)
            s.run_ms(200)
        vp.inputs.button(10, 0)                           # Shift + clic encodeur
        vp.inputs.button(6, 0)
        s.run_ms(1000)
        check('Shift + B6 1 s : rien encore', resets[0] == 0 and r('pgi') == SDRAM_BASE)
        s.run_ms(1300)
        check('Shift + B6 2 s : menu d’allumage, sans redémarrer, invité arrêté',
              resets[0] == 0 and r('menu') != 0 and r('pgi') == 0 and r('ready') == 0,
              'resets %d menu %#x pgi %#x' % (resets[0], r('menu'), r('pgi')))
        s.run_ms(2500)                                    # toujours tenus : rien de plus (verrou)
        check('… tenus encore 2,5 s : on reste au menu', resets[0] == 0 and r('menu') != 0 and r('exit_lock') == 1)
        vp.inputs.button(6, 1)
        vp.inputs.button(10, 1)
        s.run_ms(300)
        check('menu réutilisé (pas de nouvelle allocation)', r('menu') == menu0 and r('exit_lock') == 0,
              '%#x / %#x' % (r('menu'), menu0))
        vp.eve.cmd_log.clear()
        s.turn(1)                                         # (un cran : le menu se redessine)
        txt = [a[2] for n, a in vp.eve.cmd_log if n == 'TEXT' and len(a) > 2]
        check('menu : « Play+ d’origine (redemarre) »', "Play+ d'origine (redemarre)" in txt, str(txt)[:200])
        m = r('menu')
        s.turn(-5)                                        # curseur en haut : LOOM-8
        s.tap(6, after_ms=100)
        for _ in range(30):
            if r('pgi') and r('ready'):
                break
            s.run_ms(200)
        check('LOOM-8 relancé à chaud, l’image revient', r('pgi') == SDRAM_BASE and r('ready') == 1 and r('menu') == 0)
        r0 = r('renders')
        s.run_ms(500)
        check('… et son rendu audio', r('renders') - r0 > 100, str(r('renders') - r0))
        s.run_ms(1500)
        check('… et son image', r('bitmap') == 1 and r('fb') != 0)
        shift_click(2300)                                 # retour au menu
        check('2e retour au menu', r('menu') != 0 and r('pgi') == 0 and resets[0] == 0)
        shift_click(2300)                                 # depuis le menu : Play+ d'origine, samples écrasés
        boot = subprocess.run(['mtype', '-i', img, '::/Guests/boot.txt'], env=env, capture_output=True, text=True).stdout
        check('menu + Shift + B6 2 s : « ! » dans boot.txt et redémarrage (projet à recharger)',
              resets[0] >= 1 and boot == '!', 'resets %d boot %r' % (resets[0], boot))
        # 3e démarrage avec « ! » : la main est rendue tout de suite, boot.txt redevient « - ». (Le virtuel repart
        # de l'instantané, dont la FatFs garde les secteurs d'avant : on repose le fichier comme au 2e démarrage.)
        subprocess.run(['mdel', '-i', img, '::/Guests/boot.txt'], env=env, capture_output=True)
        sd_put(img, '/Guests/boot.txt', b'!')
        del s, vp
        s = open_device(sys.argv[1])
        vp = s.vp
        st = 0
        for _ in range(40):
            st = vp.r32(CFW_RAM)
            if st and vp.r32(st) == 0x4F574E31 and vp.r32(st + 4) == 0:
                break
            s.run_ms(200)
        boot = subprocess.run(['mtype', '-i', img, '::/Guests/boot.txt'], env=env, capture_output=True, text=True).stdout
        check('3e démarrage avec « ! » : Play+ d’origine aussitôt, boot.txt « - »',
              st and vp.r32(st + 4) == 0 and boot == '-', 'active %s boot %r' % (vp.r32(st + 4) if st else '?', boot))
    finally:
        subprocess.run(['mdel', '-i', img, '::/Guests/LOOM-8.pgi'], env=env, capture_output=True)
        subprocess.run(['mdel', '-i', img, '::/Guests/boot.txt'], env=env, capture_output=True)
        subprocess.run(['mdel', '-i', img, '::/Guests/dma.on', '::/Guests/dma.txt'], env=env, capture_output=True)
        subprocess.run(['mdeltree', '-i', img, '::/Guests/LOOM-8'], env=env, capture_output=True)
    return check.done()


if __name__ == '__main__':
    sys.exit(main())
