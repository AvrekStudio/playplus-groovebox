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
FIELDS = 'magic active close_menu shift phase ev_pad ev_button ev_knob ev_touch ev_encoder k a b renders held'.split()
AUDIO_OUT = 0x2000281c      # fw_audio_out : s16[2][128][2]


def gstate(vp):
    return dict(zip(FIELDS, struct.unpack('<16I', bytes(vp.mu.mem_read(GUEST, 64)))))


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
    s.shot(os.path.join(shots, 'guest_on.png'))

    # 4. sortie : Shift tenu + Back
    vp.inputs.button(10, 0)
    s.run_ms(60)
    s.tap(7)
    vp.inputs.button(10, 1)
    s.run_ms(100)
    g = gstate(vp)
    check('Shift + Back fait sortir du mode invité', g['active'] == 0, str(g))
    n = g['ev_pad']
    s.pad(0, 8)
    check('après la sortie : Polyend reprend les pads', step_on(vp, 0, 8) == 1)
    check('après la sortie : l’invité ne reçoit plus rien', gstate(vp)['ev_pad'] == n)
    s.run_ms(50)
    check('après la sortie : le son de test s’arrête', out_peak(vp) == 0, str(out_peak(vp)))
    return check.done()


if __name__ == '__main__':
    sys.exit(main())
