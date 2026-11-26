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
  * Shift + clic encodeur (B10 + B6) tenus 2 s : redémarrage (AIRCR), donc retour au menu.

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
O = dict(magic=0, active=4, buttons=100, renders=160, pgi=264, bitmap=268, msg=296, ready=308, off=312, menu=300, fb=328, fb_swap=332, pend=340, dma_off=1352)


def led(vp, i):
    """(r, g, b) de la LED du pad i, lue dans la trame encodée du pilote (tools/viewer/viewer.py)"""
    fr = bytes(vp.mu.mem_read(vp.r32(0x20004d84 + 4) + 48 * i, 48))

    def comp(o):
        return sum(1 << (7 - k) for k in range(8) if fr[o + 1 + k] == 0x07)
    return comp(16), comp(0), comp(32)


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
        mp['pad_colors'] = [{'pad_index': 150, 'pad_row': 7, 'pad_col': 10, 'color': '#ff0000'}]
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
        dmat = subprocess.run(['mtype', '-i', img, '::/Guests/dma.txt'], env=env, capture_output=True, text=True).stdout
        check('premiers transferts eDMA relus dans RAM_G : justes (/Guests/dma.txt « OK »)', ': OK' in dmat, repr(dmat[:200]))
        check('volume casque du codec à +6 dB (fw_codec_hp 0x0C)', (0x0C, 0x0C) in hp, str(hp[:3]))
        vp.eve.cmd_log.clear()
        vp.eve.max_log = 200000
        s.run_ms(400)
        txt = texts(vp)
        check('bandes latérales (charge CPU)', 'CPU' in txt, str(sorted(set(txt)))[:300])
        check('matrice lue sur la carte (bande « MAP SD »)', 'MAP' in txt and 'SD' in txt, str(sorted(set(txt)))[:300])
        check('couleur de pad de map.json (r7 c10 : rouge faible)', led(vp, 150)[0] > 0 and led(vp, 150)[1:] == (0, 0),
              str(led(vp, 150)))
        r0 = r('renders')
        s.run_ms(500)
        check('son : LOOM-8 appelé par le rendu', r('renders') - r0 > 100, str(r('renders') - r0))

        # grille éclatée : rangées 0 à 3 = les 64 pas (mesure r = rangée r), clavier = notes même sur Steps
        peak = 0
        vp.inputs.pad(5, 0, 0)                            # blanche 1 sur Steps : une note, pas un pas
        for _ in range(20):
            s.run_ms(13)
            peak = max(peak, out_peak(vp))
        vp.inputs.pad(5, 0, 1)
        s.run_ms(300)
        check('Steps : le clavier joue une note (son)', peak > 500, str(peak))
        check('Steps : le clavier ne pose pas de pas (pad du pas 1 sans trig)', led(vp, 0)[0] < 0x80, str(led(vp, 0)))
        for col in (0, 4, 8, 12):                         # pads du haut, rangée 0 : pas 1, 5, 9, 13
            s.pad(0, col)
        s.pad(2, 3)                                       # rangée 2 = mesure 3 : pas 36
        s.run_ms(300)
        s.vp.eve.screenshot(os.path.join(d, 'own_loom_grille.png'), 2)
        check('pads du haut : trigs posés (LED vive à la couleur de la piste : pas 1, 5, 9, 13, 36)',
              all(max(led(vp, p)) >= 0xf0 and led(vp, p) == led(vp, 0) for p in (0, 4, 8, 12, 43)),
              str([led(vp, p) for p in (0, 4, 8, 12, 43)]))
        check('pad de la mesure 3 : mesure 3 affichée (pas vides de la rangée 2 en bleu)',
              led(vp, 40)[2] > 0 and led(vp, 0 + 1)[2] < led(vp, 40)[2], '%s %s' % (led(vp, 40), led(vp, 1)))
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
            keys.add(tuple(led(vp, 100 + i) for i in range(16)))
        check('PLAY : LOOM-8 joue les trigs (son en sortie)', peak > 1000, str(peak))
        check('tête de lecture en blanc sur les pads, qui avance', len(heads) >= 4, str(sorted(heads)))
        check('Steps : blanches (rangée 5) fixes, sans tête de lecture', len(keys) == 1, '%d états' % len(keys))
        s.tap(7)                                          # PLAY : arrêt
        # rangée 6 (colonnes 0 à 8) = SCALE de la page Bar, sur une piste synthé seulement
        check('hors Bar : pads SCALE éteints', all(led(vp, 120 + i) == (0, 0, 0) for i in range(9)),
              str([led(vp, 120 + i) for i in range(9)]))
        lit = False
        for tr in range(1, 8):                            # pads ALGO 2 à 8 : une piste synthé, puis SEQ, SELECT + = Bar
            s.pad(tr, 16, after_ms=200)
            s.tap(9, after_ms=200)
            s.tap(3, after_ms=300)
            if any(led(vp, 120 + i) != (0, 0, 0) for i in range(9)):
                lit = True
                break
        check('Bar, piste synthé : pads SCALE allumés', lit, str([led(vp, 120 + i) for i in range(9)]))
        s.pad(6, 3, after_ms=400)                         # 4e pad SCALE : 4 doubles-croches par pas
        s.vp.eve.screenshot(os.path.join(d, 'own_loom_scale.png'), 2)
        check('pad SCALE 4 : valeur en blanc, les autres en vert',
              led(vp, 123) == (255, 255, 255) and led(vp, 120)[1] > 0 and led(vp, 120)[0] == 0,
              '%s %s' % (led(vp, 123), led(vp, 120)))
        s.pad(6, 0, after_ms=400)                         # retour à 1
        check('pad SCALE 1 : valeur changée', led(vp, 120) == (255, 255, 255) and led(vp, 123)[0] == 0,
              '%s %s' % (led(vp, 120), led(vp, 123)))
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
        s.tap(6, after_ms=100)
        for _ in range(30):
            if r('pgi') and r('ready'):
                break
            s.run_ms(200)
        check('clic : LOOM-8 chargé', r('pgi') == SDRAM_BASE and r('ready') == 1)
        s.run_ms(500)
        vp.inputs.button(10, 0)                           # Shift + clic encodeur : Play+ d'origine
        vp.inputs.button(6, 0)
        s.run_ms(1000)
        check('Shift + B6 1 s : rien encore', resets[0] == 0)
        s.run_ms(1300)
        vp.inputs.button(6, 1)
        vp.inputs.button(10, 1)
        check('Shift + B6 2 s : redémarrage (retour au menu d’allumage)', resets[0] >= 1, str(resets[0]))
    finally:
        subprocess.run(['mdel', '-i', img, '::/Guests/LOOM-8.pgi'], env=env, capture_output=True)
        subprocess.run(['mdel', '-i', img, '::/Guests/boot.txt'], env=env, capture_output=True)
        subprocess.run(['mdel', '-i', img, '::/Guests/dma.on', '::/Guests/dma.txt'], env=env, capture_output=True)
        subprocess.run(['mdeltree', '-i', img, '::/Guests/LOOM-8'], env=env, capture_output=True)
    return check.done()


if __name__ == '__main__':
    sys.exit(main())
