#!/usr/bin/env python3
"""Cible playplus-1.3.0-own : prise de contrôle du Play+ (own.c, étape 2, boucle de test).

  * la main est prise : notre table de listener, notre rendu audio, notre boucle qui tourne ;
  * pads : appui → écran + LED + une voix qui sonne ; relâché → la voix s'éteint ;
  * potards et encodeur : valeurs cumulées ; boutons tenus ; toucher des potards ignoré ;
  * l'écran affiche notre image (texte « AVREK ») et la latence événement → image est mesurée ;
  * l'application Polyend ne tourne plus (sa mise à jour n'est plus appelée).

  * à la prise de main : le menu d'allumage, d'où l'on choisit la boucle de test (pad 0).

usage: TARGET=playplus-1.3.0-own scripts/test.sh vdev_own"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'vdev'))
from device import Checks, clean_card, cstr, expected_version, open_device, work_dir  # noqa: E402
import hle  # noqa: E402

CFW_RAM = 0x20002700
LISTENER = 0x20018c34
RENDER_CB = 0x2000301c
APP_UPDATE = 0x600c84f4
# struct own (own.c)
O = dict(magic=0, active=4, pads=16, knob=36, enc=96, buttons=100, last_ev=104, lat_last=124, lat_max=128,
         lat_n=136, loops_ps=144, frames=156, renders=160, v=184, menu=300)
VOICE = 20                         # inc, phase, gain, on, pad


def texts(vp):
    return [a[2] for n, a in vp.eve.cmd_log if n == 'TEXT' and len(a) > 2]


def main():
    check = Checks()
    clean_card(sys.argv[1])                               # sans invité : la boucle de test
    s = open_device(sys.argv[1])
    vp = s.vp
    hle.enable_audio(vp)
    app_calls = [0]
    vp.hook_addr(APP_UPDATE, lambda vp: app_calls.__setitem__(0, app_calls[0] + 1))
    vp.eve.max_log = 200000
    for _ in range(50):                                   # prise de main dès le 1er tour de boucle (~2 s au virtuel)
        if vp.r32(CFW_RAM) and vp.r32(vp.r32(CFW_RAM) + 4) == 1:
            break
        s.run_ms(200)
    s.run_ms(1500)
    st = vp.r32(CFW_RAM)
    r = lambda k: vp.r32(st + O[k])                       # noqa: E731
    rs = lambda k: struct.unpack('<i', struct.pack('<I', r(k)))[0]   # noqa: E731
    check('version = ' + expected_version(), cstr(vp, vp.r32(0x600c83f0)) == expected_version())
    check('la main est prise (état, magic)', st and r('magic') == 0x4F574E31 and r('active') == 1, hex(st))
    check('prise de main avant l’écran principal de Polyend (~2,1 s au virtuel)', vp.r32(st + 12) < 2100,
          '%d ms' % vp.r32(st + 12))
    menu = vp.r32(st + 300)
    check('menu d’allumage affiché (carte sans firmware : 0 entrée)', menu != 0 and vp.r32(menu) == 0,
          '%#x n=%d' % (menu, vp.r32(menu) if menu else -1))
    check('menu : « Boucle de test », « Play+ d’origine »',
          all(x in texts(vp) for x in ('Boucle de test', "Play+ d'origine")), str(sorted(set(texts(vp))))[:200])
    vp.inputs.pad(0, 0, 0)                                # pad 0 = 1re entrée = boucle de test
    s.run_ms(100)
    vp.inputs.pad(0, 0, 1)
    s.run_ms(500)
    check('pad 0 : boucle de test choisie', vp.r32(st + 300) == 0)
    check('notre table de listener', vp.r32(LISTENER) != 0x6007b27c, hex(vp.r32(LISTENER)))
    check('notre rendu audio', vp.r32(RENDER_CB) not in (0, 0x600ad379), hex(vp.r32(RENDER_CB)))
    check('notre boucle tourne', r('loops_ps') > 100, str(r('loops_ps')))
    check('l’application Polyend ne tourne plus', app_calls[0] == 0, str(app_calls[0]))
    check('rendu audio appelé', r('renders') > 100, str(r('renders')))

    vp.eve.cmd_log.clear()
    f0, leds0 = r('frames'), getattr(vp, 'led_frames', 0)
    vp.inputs.pad(3, 10, 0)
    s.run_ms(120)
    pad = 3 * 20 + 10
    held = (vp.r32(st + O['pads'] + 4 * (pad >> 5)) >> (pad & 31)) & 1
    voices = [struct.unpack('<5I', bytes(vp.mu.mem_read(st + O['v'] + k * VOICE, VOICE))) for k in range(4)]
    playing = [v for v in voices if v[0] and v[3] == 1 and v[4] == pad]
    check('pad r3 c10 tenu', held == 1)
    check('une voix joue le pad (gain qui monte)', playing and playing[0][2] > 0, str(voices))
    check('écran redessiné', r('frames') > f0 and any('AVREK' in t for t in texts(vp)), f'{f0}->{r("frames")}')
    check('trame LED envoyée', getattr(vp, 'led_frames', 0) > leds0, f'{leds0}->{getattr(vp, "led_frames", 0)}')
    vp.inputs.pad(3, 10, 1)
    s.run_ms(120)
    voices = [struct.unpack('<5I', bytes(vp.mu.mem_read(st + O['v'] + k * VOICE, VOICE))) for k in range(4)]
    check('pad relâché : la voix s’éteint', not any(v[0] and v[4] == pad for v in voices), str(voices))

    s.knob(4, 3, after_ms=100)
    s.knob(4, -1, after_ms=100)
    s.turn(-2, after_ms=100)
    check('potard 5 : +3 -1 = 2', rs('knob') == 0 and struct.unpack('<i', struct.pack('<I', vp.r32(st + O['knob'] + 16)))[0] == 2)
    check('encodeur : -2', rs('enc') == -2, str(rs('enc')))
    vp.inputs.button(10, 0)
    s.run_ms(60)
    check('bouton 10 tenu', (r('buttons') >> 10) & 1 == 1, hex(r('buttons')))
    vp.inputs.button(10, 1)
    s.run_ms(60)
    ev = r('last_ev')
    s.touch(2, hold_ms=60, after_ms=100)
    check('toucher d’un potard ignoré', r('last_ev') == ev, f'{ev:#x} -> {r("last_ev"):#x}')
    check('latence mesurée (≤ 40 ms au virtuel)', r('lat_n') > 0 and r('lat_max') <= 40, f'n={r("lat_n")} max={r("lat_max")}')
    d = os.path.join(work_dir(), 'shots')
    os.makedirs(d, exist_ok=True)
    vp.inputs.pad(5, 17, 0)
    s.run_ms(100)
    s.shot(os.path.join(d, 'own.png'))
    vp.inputs.pad(5, 17, 1)
    s.run_ms(100)
    # Shift + B6 (clic encodeur) tenus 2 s : depuis la boucle de test, retour au menu (0.6.1) ; depuis le menu,
    # main rendue au Play+ d'origine (à chaud : la boucle de test ne touche pas aux samples)
    vp.inputs.button(10, 0)
    vp.inputs.button(6, 0)
    s.run_ms(1000)
    check('Shift + B6 1 s : toujours à nous', r('active') == 1)
    s.run_ms(1300)
    vp.inputs.button(6, 1)
    vp.inputs.button(10, 1)
    s.run_ms(300)
    check('Shift + B6 2 s (boucle de test) : retour au menu', r('active') == 1 and r('menu') != 0, f'menu {r("menu"):#x}')
    vp.inputs.button(10, 0)
    vp.inputs.button(6, 0)
    s.run_ms(2300)
    vp.inputs.button(6, 1)
    vp.inputs.button(10, 1)
    s.run_ms(800)
    check('Shift + B6 2 s (menu) : main rendue au Play+ d’origine', r('active') == 0)
    check('table de listener et rendu d’origine remis', vp.r32(LISTENER) == 0x6007b27c and vp.r32(RENDER_CB) == 0x600ad379,
          f'{vp.r32(LISTENER):#x} {vp.r32(RENDER_CB):#x}')
    check('l’application Polyend tourne de nouveau', app_calls[0] > 10, str(app_calls[0]))
    import flows
    flows.open_menu(s)
    check('son menu s’ouvre', flows.current_screen(s) == 'menu', flows.current_screen(s))
    s.shot(os.path.join(d, 'own_exit.png'))
    # RAM sale (cas de la machine) : un faux pointeur dans cfw_ram ne doit rien faire planter
    vp.mu.mem_write(CFW_RAM, struct.pack('<I', 0x4B54434B))
    m = {}
    for line in open(os.path.join(work_dir(), 'link.map')):
        p = line.split()
        if len(p) == 2:
            m[p[1]] = int(p[0], 16)
    try:
        vp.call_fn(m['cfw_own_led_blink'] | 1, 0x20004d84, 5, 1, 2, 3)
        vp.call_fn(m['cfw_own_display'] | 1, 0x20004ca0)
        ok = True
    except Exception as e:                                 # faute mémoire = exception d'Unicorn
        ok = repr(e)
    check('RAM sale au démarrage : pas de plantage', ok is True, str(ok))
    return check.done()


if __name__ == '__main__':
    sys.exit(main())
