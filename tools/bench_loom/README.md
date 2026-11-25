# Banc de charge de LOOM-8

Mesure ce que coûte le moteur audio de LOOM-8 (firmware FM-1 de Charles Vestal, sur fm1-base) sur le
Cortex-M7 du Play+ : `synth_prepare` + `synth_render` (et les effets delay / reverb) par blocs de
128 trames, comptés instruction par instruction dans Unicorn. LOOM n'est pas copié ici : le script le
clone à un commit fixé.

```bash
tools/bench_loom/build_and_run.sh        # LOOM=/chemin/vers/fm1-loom pour réutiliser un clone
```

- `bench_loom.c` : les cas (silence, chaque moteur à 1 / 4 / 6 voix, « chanson chargée »), d'après
  `tests/host/eng_bench.c` de LOOM, et un mini-runtime sans libc.
- `shim/` : `string.h` et `math.h` minimaux pour compiler sans libc.
- `run_loom.py` : chargement de l'ELF dans Unicorn et mesure ; `results_loom.json` : derniers
  résultats.

Analyse : `docs/analyses/loom-portage.md`, section « Mesures ».
