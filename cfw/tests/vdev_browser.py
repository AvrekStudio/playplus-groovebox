#!/usr/bin/env python3
"""Cible minimale : navigateur de samples (browser.c).

Menu > Samples > Load Files or Folders, avec trois WAV de test copiés sur la carte virtuelle :
  * B3 descend, B0 monte (et n'« entrent » plus) ; B3 tenu répète ;
  * auto-preview Off par défaut : se déplacer ne lance rien ;
  * Shift tenu : Back affiche « Preview Off » ; Shift + Back → « Preview On », sans quitter l'écran ;
  * auto-preview On : arriver sur un fichier lance sa pré-écoute (« dossier/nom »), pas sur « /.. » ;
  * potard 1 : volume de pré-écoute 50 % → 60 %, borné à 0 ; jauge rouge à gauche de l'écran ;
  * gain appliqué par cfw_preview_mix (appel direct) ;
  * réglages enregistrés dans /Projects/avrekBrowser ; potard 1 hors navigateur : rien.

usage: TARGET=playplus-1.3.0-base scripts/test.sh vdev_browser"""
import io
import math
import os
import struct
import subprocess
import sys
import wave

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'vdev'))
from device import Checks, cstr, open_device, work_dir  # noqa: E402
from unicorn.arm_const import UC_ARM_REG_R1  # noqa: E402
import flows  # noqa: E402

BR = 0x20002740                    # struct br_state (browser.c)
O_AUTO, O_VOL, O_PREV = 8, 0xc, 0x30
PREVIEW_PLAY = 0x600ecb80
DIR = 'avrek_test'
FILES = ['A_sine440.wav', 'B_sine880.wav', 'C_sine220.wav']
ENV = dict(os.environ, MTOOLS_SKIP_CHECK='1')


def img():
    return os.path.join(work_dir(), 'sd.img')


def mt(*a):
    return subprocess.run(list(a), capture_output=True, env=ENV)


def put_wavs():
    mt('mdeltree', '-i', img(), '::/Samples/' + DIR)
    mt('mmd', '-i', img(), '::/Samples/' + DIR)
    for i, name in enumerate(FILES):
        b = io.BytesIO()
        w = wave.open(b, 'wb')
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(44100)
        f = 220 * (i + 1)
        w.writeframes(b''.join(struct.pack('<h', int(12000 * math.sin(2 * math.pi * f * k / 44100)))
                               for k in range(22050)))
        w.close()
        p = os.path.join(work_dir(), name)
        open(p, 'wb').write(b.getvalue())
        mt('mcopy', '-o', '-i', img(), p, '::/Samples/' + DIR + '/')
        os.remove(p)


def link_sym(name):
    for line in open(os.path.join(work_dir(), 'link.map')):
        p = line.split()
        if len(p) == 2 and p[1] == name:
            return int(p[0], 16)
    raise KeyError(name)


def main():
    check = Checks()
    put_wavs()
    mt('mdel', '-i', img(), '::/Projects/avrekBrowser')
    s = open_device(sys.argv[1])
    vp = s.vp
    paths = []
    vp.hook_addr(PREVIEW_PLAY, lambda vp: paths.append(cstr(vp, vp.mu.reg_read(UC_ARM_REG_R1), 200)))
    s.run_ms(300)
    s.knob(0, 7, after_ms=200)
    check('potard 1 hors navigateur : volume inchangé (50 %)', vp.r32(BR + O_VOL) == 50, str(vp.r32(BR + O_VOL)))

    flows.open_menu(s)
    flows.enter_item(s, flows.MENU_SAMPLES)
    flows.enter_item(s, flows.SAMPLES_LOAD_FILES, after_ms=800)
    ui = flows.ui_manager(vp)
    scr = vp.r32(ui + 0x08)
    sel = lambda: vp.r32(scr + 0x5c)     # noqa: E731

    def names(n=8):
        out = []
        for i in range(n):
            p = vp.r32(vp.r32(scr + 0x68) + 4 * i)
            if not (0x20000000 <= p < 0x20080000 or 0x20200000 <= p < 0x20280000 or 0x80000000 <= p < 0x82000000):
                break
            out.append(cstr(vp, p, 64))
        return out
    check('navigateur ouvert', flows.current_screen(s) == 'browser', flows.current_screen(s))
    target = names().index('/Samples')
    s.tap(3, after_ms=250)
    check('B3 descend d’une ligne', sel() == 1, str(sel()))
    s.tap(0, after_ms=250)
    check('B0 remonte', sel() == 0 and flows.current_screen(s) == 'browser', str(sel()))
    for _ in range(target):
        s.tap(3, after_ms=200)
    check('B3 jusqu’à /Samples', names()[sel()] == '/Samples', names()[sel()])
    s.tap(6, after_ms=800)
    i = names().index('/' + DIR)
    while sel() < i:
        s.tap(3, after_ms=200)
    s.tap(6, after_ms=800)
    check('dans le dossier de test', names()[:4] == ['/..'] + FILES, str(names()[:4]))

    s.tap(3, after_ms=300)
    check('auto-preview Off par défaut : pas de pré-écoute', vp.r32(BR + O_PREV) == 0 and not paths,
          f'{vp.r32(BR + O_PREV)} {paths}')

    vp.eve.cmd_log.clear()
    vp.eve.max_log = 200000
    vp.inputs.button(10, 0)
    s.run_ms(300)
    t = [a[2] for n, a in vp.eve.cmd_log if n == 'TEXT' and len(a) > 2]
    check('Shift tenu : Back affiche « Preview Off »', 'Preview Off' in t, str(t[-6:]))
    vp.eve.cmd_log.clear()
    s.tap(7, after_ms=300)
    t = [a[2] for n, a in vp.eve.cmd_log if n == 'TEXT' and len(a) > 2]
    check('Shift + Back : « Preview On »', 'Preview On' in t and vp.r32(BR + O_AUTO) == 1, str(t[-6:]))
    vp.inputs.button(10, 1)
    s.run_ms(300)
    check('Shift + Back ne quitte pas le navigateur', flows.current_screen(s) == 'browser', flows.current_screen(s))
    s.shot(os.path.join(work_dir(), 'shots', 'browser_on.png'))

    s.tap(3, after_ms=400)
    check('auto-preview : B3 sur B_sine880 lance sa pré-écoute',
          paths and paths[-1].endswith('/' + DIR + '/B_sine880.wav'), str(paths))
    n = len(paths)
    s.tap(0, after_ms=300)
    s.tap(0, after_ms=300)
    check('B0 : A_sine440 pré-écouté, pas « /.. »',
          len(paths) == n + 1 and paths[-1].endswith('/A_sine440.wav') and sel() == 0, f'{paths[n:]} {sel()}')
    vp.inputs.button(3, 0)
    s.run_ms(1000)
    vp.inputs.button(3, 1)
    s.run_ms(200)
    check('B3 tenu : répète jusqu’en bas', sel() == 3, str(sel()))

    s.knob(0, 10, after_ms=400)
    check('potard 1 : volume 60 %', vp.r32(BR + O_VOL) == 60, str(vp.r32(BR + O_VOL)))
    d = os.path.join(work_dir(), 'shots')
    os.makedirs(d, exist_ok=True)
    shot = os.path.join(d, 'browser_vol60.png')
    s.shot(shot, scale=1)
    from PIL import Image
    im = Image.open(shot).convert('RGB')
    top = 380 - 316 * 60 // 100
    px_fill, px_track = im.getpixel((9, top + 20)), im.getpixel((9, top - 20))
    check('jauge : rouge sous le niveau, gris au-dessus',
          px_fill[0] > 200 and px_fill[1] < 120 and max(px_track) < 80, f'{px_fill} {px_track}')
    s.knob(0, -200, after_ms=300)
    check('potard 1 : borné à 0 %', vp.r32(BR + O_VOL) == 0, str(vp.r32(BR + O_VOL)))
    s.knob(0, 40, after_ms=300)

    mix = link_sym('cfw_preview_mix')
    bus, src = 0x20270000, 0x20270400
    vp.mu.mem_write(bus, b'\0' * 512)
    vp.mu.mem_write(src, struct.pack('<64I', *[(1000 & 0xffff) | ((-2000 & 0xffff) << 16)] * 64))
    vp.call_fn(mix | 1, bus, src)
    l0, r0 = struct.unpack('<ii', bytes(vp.mu.mem_read(bus, 8)))
    check('mixage à 40 % : 1000 → 400, -2000 → -800', (l0, r0) == (400, -800), f'{l0} {r0}')

    s.run_ms(1700)
    r = mt('mtype', '-i', img(), '::/Projects/avrekBrowser').stdout
    check('réglages enregistrés (auto-preview 1, volume 40)', r == b'\x01\x28', repr(r))
    s.tap(7, after_ms=500)
    check('Back seul : on quitte le navigateur', flows.current_screen(s) != 'browser', flows.current_screen(s))
    flows.close_menu(s)
    mt('mdeltree', '-i', img(), '::/Samples/' + DIR)
    mt('mdel', '-i', img(), '::/Projects/avrekBrowser')
    return check.done()


if __name__ == '__main__':
    sys.exit(main())
