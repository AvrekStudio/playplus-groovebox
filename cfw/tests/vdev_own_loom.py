#!/usr/bin/env python3
"""Cible playplus-1.3.0-own : LOOM-8 tourne sur Avrek OS (own.c + own_guest.c), sans le Play+ d'origine.

  * menu d'allumage : vide au départ (carte nettoyée), il revoit la carte toutes les 2 s et liste
    LOOM-8 ; clic de l'encodeur : chargé, choix noté dans /Guests/boot.txt ;
  * LOOM-8 envoie son image, les bandes latérales s'affichent ; page Steps : quatre blanches du clavier
    (rangée 5 de la grille 16×8, matrice docs/correspondance/loom-8.json) posent des pas, B7 (PLAY) joue ;
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
O = dict(magic=0, active=4, buttons=100, renders=160, pgi=264, bitmap=268, msg=296, ready=308, off=312, menu=300, pend=340)


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
        check('volume casque du codec à +6 dB (fw_codec_hp 0x0C)', (0x0C, 0x0C) in hp, str(hp[:3]))
        vp.eve.cmd_log.clear()
        vp.eve.max_log = 200000
        s.run_ms(400)
        txt = texts(vp)
        check('bandes latérales (charge CPU)', 'CPU' in txt, str(sorted(set(txt)))[:300])
        r0 = r('renders')
        s.run_ms(500)
        check('son : LOOM-8 appelé par le rendu', r('renders') - r0 > 100, str(r('renders') - r0))

        for col in (0, 4, 8, 12):                         # page Steps : blanches 1, 5, 9, 13 = pas 1, 5, 9, 13
            s.pad(5, col)                                 # (blanches sur la rangée 5, colonnes 0 à 15)
        s.run_ms(300)
        s.tap(7)                                          # B7 = PLAY de LOOM-8
        peak = 0
        for _ in range(150):                              # 2 s, crête toutes les ~13 ms
            s.run_ms(13)
            peak = max(peak, out_peak(vp))
        check('pas posés au clavier puis PLAY : LOOM-8 joue (son en sortie)', peak > 1000, str(peak))
        vp.eve.cmd_log.clear()
        s.run_ms(400)
        check('dernier bouton affiché (B7 relâché)', 'B7 ^' in texts(vp), str(sorted(set(texts(vp))))[:200])
        pend = [vp.r32(st + O['pend'] + 4 * i) for i in range(8)]
        check('image envoyée par tranches : plus de rangée en attente', not any(pend), str([hex(x) for x in pend]))
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
        subprocess.run(['mdeltree', '-i', img, '::/Guests/LOOM-8'], env=env, capture_output=True)
    return check.done()


if __name__ == '__main__':
    sys.exit(main())
