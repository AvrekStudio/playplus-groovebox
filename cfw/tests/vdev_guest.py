#!/usr/bin/env python3
"""Mode invité (guest.c) sur le Play+ virtuel.

  * hors mode invité : un pad pose bien un pas chez Polyend, l'invité ne reçoit rien,
  * Menu > « Mode invite » active le mode et referme le menu tout seul,
  * en mode invité : pad, bouton, potard, toucher, encodeur arrivent chez l'invité et PAS chez Polyend
    (le pad ne pose pas de pas), le son vient de l'invité (son de test non nul en sortie),
  * Shift + Back fait sortir ; Polyend reprend les entrées et le son.

usage: vdev_guest.py <image.ppf>"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from device import CFW_RAM, Checks, open_device, work_dir  # noqa: E402

GUEST = CFW_RAM + 0x40
FIELDS = 'magic active close_menu shift phase ev_pad ev_button ev_knob ev_touch ev_encoder k a b renders held frames last_frame_ms redraw p0 p1 p2 p3 p4 led_saved led_restore led_dirty led_ms led_col'.split()
AUDIO_OUT = 0x2000281c      # fw_audio_out : s16[2][128][2]


def gstate(vp):
    return dict(zip(FIELDS, struct.unpack('<29I', bytes(vp.mu.mem_read(GUEST, 116)))))


def step_on(vp, track, step):
    """le pas est-il posé chez Polyend ? (vtables des pistes / pas, voir FIRMWARE_NOTES)"""
    sel = vp.profile['app'] + 0x38 + 0x2c

    def vfn(o, off):
        return vp.r32(vp.r32(o) + off)
    pv = vp.r32(sel + 0x5c)
    trk = vp.call_fn(vfn(pv, 0), pv, track)
    st = vp.call_fn(vfn(trk, 0), trk, step)
    return vp.call_fn(vfn(st, 0x0c), st) & 0xff


def out_peak(vp):
    v = struct.unpack('<512h', bytes(vp.mu.mem_read(AUDIO_OUT, 1024)))
    return max(abs(x) for x in v)


C_BG, C_LIT = (0x10, 0x14, 0x18), (0xff, 0xb0, 0x00)   # palette de guest_screen.c


def px(vp, x, y):
    return vp.eve.renderer.image(1).convert('RGB').getpixel((x, y))


LEDS = 0x20004d84          # pilote LED des pads (guest_led.c)


def led(vp, i):
    """couleur (r, g, b) de la LED i lue dans la trame encodée : 3 x 16 o (vert, rouge, bleu),
    l'octet 1 + k porte le bit 7 - k (0x07 = 1, 0x3f = 0)"""
    fr = bytes(vp.mu.mem_read(vp.r32(LEDS + 4) + 48 * i, 48))

    def comp(o):
        return sum(1 << (7 - k) for k in range(8) if fr[o + 1 + k] == 0x07)
    return comp(16), comp(0), comp(32)


def cell_center(row, col):
    return 10 + col * 15 + 6, 56 + row * 15 + 6


def main():
    check = Checks()
    s = open_device(sys.argv[1])
    vp = s.vp
    import flows
    import hle
    hle.enable_audio(vp)
    shots = os.path.join(work_dir(), 'shots')
    os.makedirs(shots, exist_ok=True)

    # 1. hors mode invité
    s.run_ms(200)
    before = step_on(vp, 0, 4)
    s.pad(0, 4)
    check('hors invité : le pad pose un pas chez Polyend', step_on(vp, 0, 4) != before)
    g = gstate(vp)
    check('hors invité : l’invité ne reçoit rien', g['active'] == 0 and g['ev_pad'] == 0, str(g))
    check('hors invité : silence en sortie (projet vide)', out_peak(vp) == 0, str(out_peak(vp)))
    s.run_ms(100)
    polyend_led4 = led(vp, 4)
    check('hors invité : le pas posé allume sa LED (lue dans la trame)', polyend_led4 != (0, 0, 0), str(polyend_led4))

    # 2. activation par le menu
    flows.open_menu(s)
    flows.enter_item(s, 7, after_ms=600)
    g = gstate(vp)
    check('Menu > Mode invite active le mode', g['active'] == 1, str(g))
    check('le menu se referme tout seul', flows.current_screen(s) != 'menu', flows.current_screen(s))

    # 3. en mode invité
    s.pad(0, 6)
    s.tap(8)                     # Play
    s.knob(3, 2)
    s.touch(3)
    s.turn(1)
    g = gstate(vp)
    check('pad, bouton, potard, toucher, encodeur arrivent chez l’invité',
          (g['ev_pad'], g['ev_button'], g['ev_knob'], g['ev_touch'], g['ev_encoder']) == (2, 2, 1, 2, 1), str(g))
    check('dernier événement = encodeur +1', (g['k'], g['b']) == (4, 1), str(g))
    check('Polyend ne reçoit pas le pad (pas non posé)', step_on(vp, 0, 6) == 0)
    s.run_ms(100)
    g = gstate(vp)
    check('le son vient de l’invité (blocs rendus)', g['renders'] > 20, str(g['renders']))
    check('son de test présent en sortie', out_peak(vp) > 1000, str(out_peak(vp)))

    # 3 bis. l'écran est à l'invité
    sw = vp.eve.swaps
    s.run_ms(300)
    g = gstate(vp)
    check('l’invité dessine ses images (~30/s)', 5 <= g['frames'] and vp.eve.swaps - sw >= 5,
          'frames %d, swaps %d' % (g['frames'], vp.eve.swaps - sw))
    check('fond de la mire à l’écran', px(vp, 2, 2) == C_BG, str(px(vp, 2, 2)))
    vp.inputs.pad(2, 5, 0)                         # pad tenu
    s.run_ms(150)
    check('un pad tenu s’allume sur la grille', px(vp, *cell_center(2, 5)) == C_LIT, str(px(vp, *cell_center(2, 5))))
    s.shot(os.path.join(shots, 'guest_on.png'))
    check('LED : le pad tenu est orange', led(vp, 45) == (255, 96, 0), str(led(vp, 45)))
    check('LED : la trame de Polyend est remplacée', led(vp, 4) != polyend_led4, str(led(vp, 4)))
    c0 = gstate(vp)['led_col']
    s.run_ms(300)
    c1 = gstate(vp)['led_col']
    check('LED : la tête de lecture avance', c1 != c0 and led(vp, 60 + c1) == (0, 40, 48), '%d -> %d %s' % (c0, c1, led(vp, 60 + c1)))
    vp.inputs.pad(2, 5, 1)
    s.run_ms(150)
    check('relâché, il s’éteint', px(vp, *cell_center(2, 5)) != C_LIT, str(px(vp, *cell_center(2, 5))))

    # 4. sortie : Shift tenu + Back
    vp.inputs.button(10, 0)
    s.run_ms(60)
    s.tap(7)
    vp.inputs.button(10, 1)
    s.run_ms(100)
    g = gstate(vp)
    check('Shift + Back fait sortir du mode invité', g['active'] == 0, str(g))
    s.run_ms(200)
    check('Polyend a redessiné son écran', px(vp, 2, 2) != C_BG and gstate(vp)['redraw'] == 0, str(px(vp, 2, 2)))
    s.shot(os.path.join(shots, 'guest_off.png'))
    check('LED : Polyend retrouve sa trame', led(vp, 4) == polyend_led4 and gstate(vp)['led_restore'] == 0,
          '%s / %s' % (led(vp, 4), polyend_led4))
    n = g['ev_pad']
    s.pad(0, 8)
    check('après la sortie : Polyend reprend les pads', step_on(vp, 0, 8) == 1)
    check('après la sortie : l’invité ne reçoit plus rien', gstate(vp)['ev_pad'] == n)
    s.run_ms(50)
    check('après la sortie : le son de test s’arrête', out_peak(vp) == 0, str(out_peak(vp)))
    return check.done()


if __name__ == '__main__':
    sys.exit(main())
