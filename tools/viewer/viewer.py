#!/usr/bin/env python3
"""Play+ virtuel en fenêtre : l'écran en direct, les 160 pads avec leurs LED, les 15 potards et les
boutons, à la souris et au clavier. Pour se promener dans les menus, l'Explorateur, le mode invité et
LOOM-8 sans la machine.

    python3 tools/viewer/viewer.py [image.ppf] [--guests DOSSIER] [--vitesse MS]

- image.ppf : par défaut le dernier build (cfw/build/playplus-1.3.0/PlayPlus_Avrek.ppf).
- --guests : copie les .pgi de ce dossier dans /Guests de la carte virtuelle (ex. dist/Guests).
- --vitesse : millisecondes de machine simulées par rafraîchissement (20 par défaut).

Le Play+ virtuel est environ 25 fois plus lent que la vraie machine, et le son n'est pas joué : c'est un
outil pour voir et naviguer, pas pour jouer. Le premier lancement d'une image démarre la machine
(environ 10 min), les suivants repartent de l'instantané (quelques secondes).

Souris : clic sur un pad = appui tant que le bouton est tenu ; molette sur un potard = le tourner, clic
= le toucher ; molette sur l'écran = l'encodeur ; clic sur un bouton du bandeau = appui.
Clavier : Espace = Play (8), Échap = Back (7), Entrée = Menu / clic de l'encodeur (6), Maj = Shift (10),
flèches gauche / droite = encodeur, F12 = capture d'écran (PNG dans le dossier courant).

Sans fenêtre (vérification, CI) : --essai capture.png fait quelques appuis et enregistre le panneau."""
import argparse
import os
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..', '..')
sys.path.insert(0, os.path.join(ROOT, 'cfw', 'tests'))
sys.path.insert(0, os.path.join(ROOT, 'cfw', 'vdev'))

from PIL import Image, ImageDraw  # noqa: E402

LEDS = 0x20004d84                 # pilote LED des pads (cfw/src/guest_led.c)
SCALE = 2                         # écran 320 x 240 affiché en 640 x 480
CELL = 30                         # un pad
KNOB = 40
W = 20 * CELL + 40
SCREEN_Y = 10
PADS_Y = SCREEN_Y + 240 * SCALE + 14
KNOBS_Y = PADS_Y + 8 * CELL + 14
BTNS_Y = KNOBS_Y + KNOB + 30
H = BTNS_Y + 40
SCREEN_X = (W - 320 * SCALE) // 2
# boutons connus (identifiants de l'officiel, voir docs/analyses/materiel-entrees-sorties.md)
BUTTONS = [(0, 'G1'), (3, 'G2'), (6, 'Menu'), (7, 'Back'), (8, 'Play'), (9, 'Soft3'), (10, 'Shift'),
           (1, '1'), (2, '2'), (4, '4'), (5, '5'), (11, '11'), (12, '12'), (13, '13'), (14, '14'), (15, '15')]


def led(vp, i):
    """(r, g, b) de la LED i, lue dans la trame encodée (voir guest_led.c)"""
    fr = bytes(vp.mu.mem_read(vp.r32(LEDS + 4) + 48 * i, 48))

    def comp(o):
        return sum(1 << (7 - k) for k in range(8) if fr[o + 1 + k] == 0x07)
    return comp(16), comp(0), comp(32)


def compose(vp, held=()):
    """le panneau entier : écran, pads (couleur de leur LED), potards, boutons"""
    img = Image.new('RGB', (W, H), (24, 24, 28))
    d = ImageDraw.Draw(img)
    if vp.eve.renderer:
        img.paste(vp.eve.renderer.image(SCALE).convert('RGB'), (SCREEN_X, SCREEN_Y))
    for i in range(160):
        r, c = divmod(i, 20)
        x, y = 20 + c * CELL, PADS_Y + r * CELL
        col = led(vp, i)
        base = tuple(max(v, 40) for v in col) if any(col) else (48, 48, 54)
        d.rounded_rectangle((x + 2, y + 2, x + CELL - 3, y + CELL - 3), 4, fill=base,
                            outline=(255, 255, 255) if i in held else None, width=2)
    for k in range(15):
        x = 20 + k * KNOB
        d.ellipse((x + 4, KNOBS_Y, x + KNOB - 4, KNOBS_Y + KNOB - 8), fill=(70, 70, 78), outline=(150, 150, 160))
        d.text((x + KNOB // 2 - 6, KNOBS_Y + KNOB - 4), str(k), fill=(170, 170, 180))
    for n, (bid, name) in enumerate(BUTTONS):
        x = 20 + n * (W - 40) // len(BUTTONS)
        d.rounded_rectangle((x + 2, BTNS_Y, x + (W - 40) // len(BUTTONS) - 4, BTNS_Y + 28), 5,
                            fill=(90, 60, 30) if bid == 8 else (60, 60, 68))
        d.text((x + 6, BTNS_Y + 8), name, fill=(230, 230, 230))
    return img


def where(x, y):
    """ce qu'il y a sous la souris : ('pad', i) / ('knob', k) / ('button', id) / ('screen', 0) / None"""
    if SCREEN_X <= x < SCREEN_X + 320 * SCALE and SCREEN_Y <= y < SCREEN_Y + 240 * SCALE:
        return 'screen', 0
    if 20 <= x < 20 + 20 * CELL and PADS_Y <= y < PADS_Y + 8 * CELL:
        return 'pad', (y - PADS_Y) // CELL * 20 + (x - 20) // CELL
    if 20 <= x < 20 + 15 * KNOB and KNOBS_Y <= y < KNOBS_Y + KNOB:
        return 'knob', (x - 20) // KNOB
    if 20 <= x < W - 20 and BTNS_Y <= y < BTNS_Y + 28:
        return 'button', BUTTONS[min((x - 20) * len(BUTTONS) // (W - 40), len(BUTTONS) - 1)][0]
    return None


def default_ppf():
    return os.path.join(ROOT, 'cfw', 'build', 'playplus-1.3.0', 'PlayPlus_Avrek.ppf')


def open_machine(ppf, guests):
    """instantané de démarrage de l'image (créé au besoin), puis la carte : /Guests"""
    from device import open_device, snapshot_for, work_dir
    os.environ.setdefault('VPLAY_DIR', os.path.join(os.path.expanduser('~'), '.cache', 'vplay', 'viewer'))
    wd = work_dir()
    img = os.path.join(wd, 'img.ppf')
    if not os.path.exists(img) or open(img, 'rb').read() != open(ppf, 'rb').read():
        shutil.copy(ppf, img)                      # une autre image : nouvel instantané
        for f in os.listdir(wd):
            if f.endswith('.snap'):
                os.remove(os.path.join(wd, f))
    snapshot_for(img)
    if guests:
        env = dict(os.environ, MTOOLS_SKIP_CHECK='1')
        card = os.path.join(wd, 'sd.img')
        subprocess.run(['mmd', '-i', card, '::/Guests'], env=env, capture_output=True)
        for f in sorted(os.listdir(guests)):
            if f.lower().endswith('.pgi'):
                subprocess.run(['mcopy', '-o', '-i', card, os.path.join(guests, f), '::/Guests/' + f],
                               env=env, check=True)
                print('carte : /Guests/' + f)
    s = open_device(img)
    import hle
    hle.enable_audio(s.vp)
    return s


def essai(s, out):
    """sans fenêtre : ouvre le menu, descend, referme, appuie sur un pad, enregistre le panneau"""
    import flows
    flows.open_menu(s)
    s.turn(3)
    flows.close_menu(s)
    s.pad(0, 2)
    s.run_ms(200)
    compose(s.vp).save(out)
    print('panneau enregistré :', out)


def window(s, step_ms):
    import tkinter as tk
    from PIL import ImageTk
    vp = s.vp
    root = tk.Tk()
    root.title('Play+ virtuel — AvrekStudio')
    canvas = tk.Canvas(root, width=W, height=H, highlightthickness=0)
    canvas.pack()
    state = {'held': set(), 'img': None, 'pressed': None, 'shift': False}

    def refresh():
        t0 = time.time()
        s.run_ms(step_ms)
        state['img'] = ImageTk.PhotoImage(compose(vp, state['held']))
        canvas.delete('all')
        canvas.create_image(0, 0, anchor='nw', image=state['img'])
        root.title(f'Play+ virtuel — {vp.ms / 1000:.1f} s machine — {step_ms / 1000 / max(time.time() - t0, 1e-3):.2f}x')
        root.after(1, refresh)

    def down(e):
        w = where(e.x, e.y)
        state['pressed'] = w
        if not w:
            return
        kind, i = w
        if kind == 'pad':
            vp.inputs.pad(i // 20, i % 20, 0)
            state['held'].add(i)
        elif kind == 'knob':
            vp.inputs.touch(i, 0)
        elif kind == 'button':
            vp.inputs.button(i, 0)
        elif kind == 'screen':
            vp.inputs.button(6, 0)

    def up(_e):
        w, state['pressed'] = state['pressed'], None
        if not w:
            return
        kind, i = w
        if kind == 'pad':
            vp.inputs.pad(i // 20, i % 20, 1)
            state['held'].discard(i)
        elif kind == 'knob':
            vp.inputs.touch(i, 1)
        elif kind == 'button':
            vp.inputs.button(i, 1)
        elif kind == 'screen':
            vp.inputs.button(6, 1)

    def wheel(e):
        delta = 1 if (getattr(e, 'delta', 0) > 0 or getattr(e, 'num', 0) == 4) else -1
        w = where(e.x, e.y)
        if w and w[0] == 'knob':
            vp.inputs.knob(w[1], delta)
        else:
            vp.inputs.encoder(delta)

    def key(e, state_):
        k = e.keysym
        btn = {'space': 8, 'Escape': 7, 'Return': 6, 'Shift_L': 10, 'Shift_R': 10}.get(k)
        if btn is not None:
            vp.inputs.button(btn, state_)
        elif state_ == 0 and k in ('Left', 'Right'):
            vp.inputs.encoder(-1 if k == 'Left' else 1)
        elif state_ == 0 and k == 'F12':
            name = time.strftime('playplus_%H%M%S.png')
            vp.eve.screenshot(name, 1)
            print('capture :', name)

    canvas.bind('<ButtonPress-1>', down)
    canvas.bind('<ButtonRelease-1>', up)
    canvas.bind('<MouseWheel>', wheel)              # Windows, macOS
    canvas.bind('<Button-4>', wheel)                # Linux
    canvas.bind('<Button-5>', wheel)
    root.bind('<KeyPress>', lambda e: key(e, 0))
    root.bind('<KeyRelease>', lambda e: key(e, 1))
    root.after(1, refresh)
    root.mainloop()


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('ppf', nargs='?', default=default_ppf())
    ap.add_argument('--guests')
    ap.add_argument('--vitesse', type=int, default=20)
    ap.add_argument('--essai')
    a = ap.parse_args()
    if not os.path.exists(a.ppf):
        sys.exit(f'{a.ppf} introuvable : lancer d’abord scripts/test.sh (build) ou scripts/package.sh')
    s = open_machine(os.path.abspath(a.ppf), a.guests)
    if a.essai:
        essai(s, a.essai)
    else:
        window(s, a.vitesse)


if __name__ == '__main__':
    main()
