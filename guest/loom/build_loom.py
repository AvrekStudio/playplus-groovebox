#!/usr/bin/env python3
"""Construit LOOM pour le Play+ depuis son fork, AvrekStudio/avrek-loom (privé, GPL-3.0) : guest/loom/loom.pgi
par défaut. Depuis le 27/11/2026, LOOM n'est plus un invité « tel quel » : c'est la base du grand projet, et son
code (avec notre couche Play+, playplus/plat_playplus.c) vit et se modifie dans ce fork. Ce dépôt-ci garde
l'hôte (Avrek OS), le SDK d'invité, la matrice (docs/correspondance/loom-8.json), gen_map.py, loom.ld, shim/ et
le Play+ virtuel ; le fork s'en sert (variable PPG).

usage: guest/loom/build_loom.py [sortie.pgi]

Le fork est cherché dans $AVREK_LOOM, puis à côté de ce dépôt (../avrek-loom), puis ~/src/avrek-loom ; sinon
il est cloné dans ~/src/avrek-loom. Les objets et loom.elf vont dans guest/loom/build (les tests y lisent les
symboles)."""
import importlib.util
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
URL = 'https://github.com/AvrekStudio/avrek-loom'


def fork_dir():
    for d in (os.environ.get('AVREK_LOOM'), os.path.join(os.path.dirname(ROOT), 'avrek-loom'),
              os.path.expanduser('~/src/avrek-loom')):
        if d and os.path.exists(os.path.join(d, 'playplus', 'build.py')):
            return d
    d = os.path.expanduser('~/src/avrek-loom')
    subprocess.run(['git', 'clone', '-q', URL, d], check=True)
    return d


def build(out=os.path.join(HERE, 'loom.pgi')):
    os.environ['PPG'] = ROOT
    src = os.path.join(fork_dir(), 'playplus', 'build.py')
    spec = importlib.util.spec_from_file_location('avrek_loom_build', src)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod.build(out, bdir=os.path.join(HERE, 'build'))


if __name__ == '__main__':
    build(sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, 'loom.pgi'))
