#!/usr/bin/env python3
"""Cible playplus-1.3.0-own (0.6.3) : mise à jour du firmware depuis le menu d'allumage, sans l'officiel.

Ce que fait l'officiel (relevé au virtuel le 27/11/2026 sur l'image « base », Settings > Firmware > Update
Firmware, PlayPlus_Test.ppf, Yes) : la fonction en 0x600b11f8 reçoit le NOM du fichier, écrit exactement
b'PlayPlus_Test.ppf' (sans chemin ni fin de ligne) dans /Firmware/_fwinfo, puis redémarre (boucle en 0x600b11d4).
Le chargeur de Polyend fait le reste au démarrage (pas émulé : à confirmer sur la machine).

  * menu d'allumage : 4e entrée (carte sans invité : 3e) « Mise a jour du firmware » ;
  * page 1 : seuls les PlayPlus*.ppf de /Firmware (pas Autre.ppf, pas PlayPlus_note.txt), puis « Retour » ;
  * page 2 : « Non, retour » d'abord ; « Non » revient à la liste ;
  * Shift + clic 2 s sur une page de mise à jour : retour au menu (pas au Play+ d'origine) ;
  * « Oui » : /Firmware/_fwinfo contient exactement le nom (comme l'officiel), puis SYSRESETREQ.

usage: TARGET=playplus-1.3.0-own scripts/test.sh vdev_own_maj"""
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'vdev'))
from device import CFW_RAM, Checks, clean_card, cstr, open_device, work_dir  # noqa: E402
from vdev_guest_sd import sd_put  # noqa: E402
from unicorn import UC_HOOK_MEM_WRITE  # noqa: E402

MENU = 300                          # struct own : menu (own.h)
# struct own_menu (own.h) : n, cur, names[16][24], page, nf, fsel, fw[8][32]
M_CUR, M_PAGE, M_NF, M_FW = 4, 8 + 16 * 24, 8 + 16 * 24 + 4, 8 + 16 * 24 + 12
AIRCR = 0xE000ED0C
NAME = 'PlayPlus_Test.ppf'


def texts(vp):
    return [a[2] for n, a in vp.eve.cmd_log if n == 'TEXT' and len(a) > 2]


def card(path):
    r = subprocess.run(['mtype', '-i', os.path.join(work_dir(), 'sd.img'), '::' + path], capture_output=True,
                       env=dict(os.environ, MTOOLS_SKIP_CHECK='1'))
    return r.stdout if r.returncode == 0 else None


def main():
    check = Checks()
    clean_card(sys.argv[1])
    img = os.path.join(work_dir(), 'sd.img')
    env = dict(os.environ, MTOOLS_SKIP_CHECK='1')
    subprocess.run(['mdeltree', '-i', img, '::/Firmware'], env=env, capture_output=True)
    subprocess.run(['mmd', '-i', img, '::/Firmware'], env=env, capture_output=True)
    for nm, data in ((NAME, b':00000001FF\r\n'), ('Autre.ppf', b':00000001FF\r\n'), ('PlayPlus_note.txt', b'x'),
                     ('_fwinfo', b'ancien contenu plus long que le nom')):
        sd_put(img, '/Firmware/' + nm, data)
    s = open_device(sys.argv[1])
    vp = s.vp
    resets = []
    vp.mu.hook_add(UC_HOOK_MEM_WRITE, lambda mu, acc, addr, size, val, _: resets.append(val),
                   begin=AIRCR, end=AIRCR + 3)
    vp.eve.max_log = 200000
    for _ in range(50):
        if vp.r32(CFW_RAM) and vp.r32(vp.r32(CFW_RAM) + 4) == 1:
            break
        s.run_ms(200)
    s.run_ms(800)
    st = vp.r32(CFW_RAM)
    m = vp.r32(st + MENU)
    page = lambda: vp.r32(m + M_PAGE)          # noqa: E731
    cur = lambda: vp.r32(m + M_CUR)            # noqa: E731

    def click(after_ms=600):
        vp.eve.cmd_log.clear()
        s.tap(6, after_ms=after_ms)

    n = vp.r32(m)
    vp.eve.cmd_log.clear()
    s.turn(5, after_ms=400)                    # curseur en bas : la dernière entrée
    check('menu : « Mise a jour du firmware » en dernier', cur() == n + 2 and 'Mise a jour du firmware' in texts(vp),
          f'n={n} cur={cur()} {sorted(set(texts(vp)))[:12]}')
    click()
    shots = os.path.join(work_dir(), 'shots')
    os.makedirs(shots, exist_ok=True)
    s.vp.eve.screenshot(os.path.join(shots, 'own_maj_liste.png'), 2)
    check('page 1 : un seul .ppf retenu (PlayPlus*.ppf)', page() == 1 and vp.r32(m + M_NF) == 1 and
          cstr(vp, m + M_FW) == NAME, f'page={page()} nf={vp.r32(m + M_NF)} {cstr(vp, m + M_FW)}')
    check('page 1 : le nom et « Retour » à l’écran', NAME in texts(vp) and 'Retour' in texts(vp),
          str(sorted(set(texts(vp))))[:200])
    # Shift + clic tenus 2 s : retour au menu (pas au Play+ d'origine)
    vp.inputs.button(10, 0)
    s.run_ms(50)
    vp.inputs.button(6, 0)
    s.run_ms(2300)
    vp.inputs.button(6, 1)
    vp.inputs.button(10, 1)
    s.run_ms(400)
    check('Shift + clic sur la liste : retour au menu', page() == 0 and vp.r32(st + 4) == 1 and cur() == n + 2,
          f'page={page()} actif={vp.r32(st + 4)} cur={cur()}')
    click()
    click()                                    # le .ppf : confirmation
    s.vp.eve.screenshot(os.path.join(shots, 'own_maj_confirmer.png'), 2)
    check('page 2 : confirmation, « Non » d’abord', page() == 2 and cur() == 0 and
          'Oui, installer ce firmware' in texts(vp) and NAME in texts(vp), f'page={page()} cur={cur()}')
    click()                                    # « Non, retour »
    check('« Non » : retour à la liste', page() == 1 and not resets, f'page={page()} resets={resets}')
    click()
    s.turn(1, after_ms=300)                    # « Oui »
    click(after_ms=1500)
    check('« Oui » : /Firmware/_fwinfo = le nom seul, comme l’officiel', card('/Firmware/_fwinfo') == NAME.encode(),
          repr(card('/Firmware/_fwinfo')))
    check('puis redémarrage (SYSRESETREQ)', 0x05FA0004 in resets, str([hex(v) for v in resets]))
    for nm in (NAME, 'Autre.ppf', 'PlayPlus_note.txt', '_fwinfo'):
        subprocess.run(['mdel', '-i', img, '::/Firmware/' + nm], env=env, capture_output=True)
    return check.done()


if __name__ == '__main__':
    sys.exit(main())
