#!/usr/bin/env python3
"""Mesure de la charge audio (audio_load.c) sur le Play+ virtuel.

  * notre chronomètre prend la place du callback de rendu et garde l'original (0x600ad379),
  * chaque bloc audio passe par lui (compteur de blocs = blocs injectés par l'émulateur),
  * il appelle bien le rendu d'origine en entier : les cycles qu'il mesure valent, à quelques
    instructions près, ce que l'émulateur compte pour toute la routine audio,
  * les fenêtres d'1 s sont publiées (moyenne et pic),
  * une valeur de charge plausible (entre 1 % et 100 % du budget de 2,902 ms).

L'émulateur tourne en mode comptage exact (vplay.exact_count) : 1 instruction = 1 cycle. C'est un
ordre de grandeur ; seule la vraie machine donne la vraie charge.

usage: vdev_audio.py <image.ppf>"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from device import CFW_RAM, Checks, open_device  # noqa: E402

AUDIO = CFW_RAM + 0x10
MAGIC = 0x41564B41
RENDER_CB = 0x2000301c
STOCK_RENDER = 0x600ad379
AUDIO_CB = 0x00013a18
BUDGET = 1741497
FIELDS = ('magic orig installs blocks last peak_all win_sum win_peak win_n avg_done peak_done windows').split()


def audio_state(vp):
    return dict(zip(FIELDS, struct.unpack('<12I', bytes(vp.mu.mem_read(AUDIO, 48)))))


def cfw_symbol(name):
    for ln in open(os.path.join(os.environ['VPLAY_DIR'], 'link.map')):
        p = ln.split()
        if len(p) == 2 and p[1] == name:
            return int(p[0], 16)


def main():
    check = Checks()
    s = open_device(sys.argv[1])
    vp = s.vp
    import hle
    hle.enable_audio(vp)
    vp.exact_count(True)

    # mesure côté émulateur : instructions de toute la routine audio, bloc par bloc
    costs, orig_call = [], vp.call_fn

    def call_fn(addr, *a, **k):
        n0 = vp.exact_insns
        r = orig_call(addr, *a, **k)
        if addr == AUDIO_CB | 1:
            costs.append((vp.exact_insns - n0, struct.unpack('<I', bytes(vp.mu.mem_read(AUDIO + 16, 4)))[0]))
        return r
    vp.call_fn = call_fn

    s.run_ms(50)
    st = audio_state(vp)
    mine = cfw_symbol('cfw_audio_render')
    check('état audio initialisé', st['magic'] == MAGIC, hex(st['magic']))
    check('le pointeur de rendu désigne notre chronomètre', vp.r32(RENDER_CB) == mine,
          f'{vp.r32(RENDER_CB):#x} au lieu de {mine:#x}')
    check('l’original est mémorisé (0x600ad379)', st['orig'] == STOCK_RENDER, hex(st['orig']))

    b0 = st['blocks']
    n0 = len(costs)
    s.run_ms(1200)
    st = audio_state(vp)
    injected = len(costs) - n0
    check('chaque bloc passe par notre code', st['blocks'] - b0 == injected, f'{st["blocks"] - b0} vs {injected}')
    check('une fenêtre d’1 s au moins est publiée', st['windows'] >= 1, str(st['windows']))
    check('installé une seule fois (pas de bagarre de pointeur)', st['installs'] == 1, str(st['installs']))

    # notre mesure (cycles du rendu seul) vs l'émulateur (toute la routine) : même ordre, un peu moins
    ratios = [m / total for total, m in costs[-50:] if total]
    worst = min(ratios), max(ratios)
    check('le rendu d’origine est appelé en entier (mesure = 90 à 100 % de la routine)',
          0.90 <= worst[0] and worst[1] <= 1.0, f'{worst[0]:.3f}..{worst[1]:.3f}')

    avg = st['avg_done']
    load = 100 * avg / BUDGET
    peak = 100 * st['peak_done'] / BUDGET
    print(f'   charge (émulateur, 1 instr = 1 cycle) : moyenne {load:.1f} %, pic {peak:.1f} %, '
          f'dernier bloc {st["last"]} cycles')
    check('charge plausible (1 à 100 %)', 1 <= load <= 100, f'{load:.1f} %')
    return check.done()


if __name__ == '__main__':
    sys.exit(main())
