"""Outils communs aux tests sur le Play+ virtuel (cfw/vdev).

Un test démarre l'image patchée une fois (environ 10 min), garde un instantané dans $VPLAY_DIR, puis
repart de cet instantané à chaque lancement. Il pilote ensuite la machine par ses propres entrées
(boutons, pads, encodeur, potards) et lit sa mémoire et son écran."""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..', '..')
sys.path.insert(0, os.path.join(HERE, '..', 'vdev'))

BOOT_MS = 23000                 # assez pour passer l'écran de démarrage et charger le projet
VERSION_GETTER = 0x600c83ed     # fonction de l'officiel qui renvoie la chaîne de version (Thumb)
CFW_RAM = 0x20002700            # notre RAM de travail (voir symbols.ld)


def work_dir():
    d = os.environ.setdefault('VPLAY_DIR', os.path.join('/tmp', 'vplay-' + str(os.getuid())))
    os.makedirs(d, exist_ok=True)
    return d


def snapshot_for(ppf):
    """Instantané de démarrage de cette image, créé s'il manque."""
    snap = os.path.join(work_dir(), os.path.basename(ppf) + '.boot.snap')
    if not os.path.exists(snap):
        import stage
        print(f'démarrage de {os.path.basename(ppf)} jusqu’à {BOOT_MS} ms (une fois, ~10 min) ...', flush=True)
        stage.stage(ppf, None, snap, insns=BOOT_MS * 100_000, seconds=3600, quiet=True)
    return snap


def clean_card(ppf):
    """Carte virtuelle sans invités (/Guests, /guest.pgi) : un essai précédent (la fenêtre viewer.py
    pointée sur ce dossier, un test interrompu) ne doit pas changer ce que voit le test."""
    import subprocess
    snapshot_for(ppf)
    img = os.path.join(work_dir(), 'sd.img')
    env = dict(os.environ, MTOOLS_SKIP_CHECK='1')
    subprocess.run(['mdeltree', '-i', img, '::/Guests'], env=env, capture_output=True)
    subprocess.run(['mdel', '-i', img, '::/guest.pgi'], env=env, capture_output=True)


def open_device(ppf):
    from session import Session
    return Session(snapshot_for(ppf), ppf=ppf)


def expected_version(target=None):
    target = target or os.environ.get('TARGET', 'playplus-1.3.0')
    h = open(os.path.join(ROOT, 'cfw', 'targets', target, 'target.h')).read()
    return re.search(r'#define CFW_VERSION_STRING "([^"]+)"', h).group(1)


def cstr(vp, addr, limit=64):
    b = bytes(vp.mu.mem_read(addr, limit))
    return b.split(b'\0')[0].decode('latin-1')


class Checks:
    def __init__(self):
        self.results = []

    def __call__(self, name, ok, detail=''):
        self.results.append(bool(ok))
        print(f"{'PASS' if ok else 'FAIL'} {name}" + (f' : {detail}' if detail and not ok else ''), flush=True)

    def done(self):
        n, ok = len(self.results), sum(self.results)
        print(f'{ok}/{n} passed')
        return 0 if ok == n else 1
