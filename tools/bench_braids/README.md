# Banc CPU de Braids

Mesure les instructions par échantillon des 48 modèles de Braids (Mutable Instruments, MIT), compilés
pour le Cortex-M7 du Play+. Le code de Braids n'est pas copié ici : le script le clone à un commit
fixé.

```bash
tools/bench_braids/build_and_run.sh      # EURO=/chemin/vers/eurorack pour réutiliser un clone
```

- `bench.cc` : point d'entrée (init, choix du modèle, rendu par tranches de 24 échantillons) et
  mini-runtime (`memset`, `memcpy`…).
- `shim/` : en-têtes de substitution pour compiler Braids sans libc.
- `link.ld` : code en 0x60200000, RAM en 0x20200000 (adresses du banc, pas celles du Play+).
- `run.py` : charge l'ELF dans Unicorn, compte les instructions exécutées, écrit `results.json`.

Résultats et analyse : `docs/analyses/braids.md`.
