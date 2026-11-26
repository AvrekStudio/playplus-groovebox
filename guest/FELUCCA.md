# Firmwares de la famille Felucca sur le Play+ (Avrek OS 0.6.0)

Neuf firmwares du FM-1 tournent en invités PGI, **sans toucher à leur code** : on compile leurs propres sources
comme le font leurs tests ou leur émulateur web, contre le Play+. Licences GPL-3.0 : les `.pgi` sont pour un usage
personnel, ne pas les distribuer.

| Invité | Dépôt (commit) | Comment | Mémoire | Sauvegarde |
| --- | --- | --- | --- | --- |
| SLOOP | isod89/sloop-fm1 (`bac9c05`) | sources dans l'ordre de `tests/ui_pages_test.c` → `sdk/felucca_pgi.h` | 2 Mo | non |
| ORBIT | dspaudio/orbit (`1dbb77d`) | son harnais `tests/orbit_host_support.h` → `felucca_pgi.h` | 2 Mo | non |
| KARMA | eatyourgreensstudio-dotcom/karma-fm1 (`1d67ab9`) | `tests/ui_pages_test.c` (+ Plaits / Elements en C++) → `felucca_pgi.h` | 2 Mo | non |
| Jangada | zednaked/jangada (`b546675`) | sources dans l'ordre de `tests/layers_test.c` → `felucca_pgi.h` | 2 Mo | non |
| Skiff | illuminus5/skiff (`392c1df`) | son émulateur web `web/emu/felucca_web.c` → `sdk/webapi_pgi.h` | 8 Mo | flash.bin |
| Melodee | keremimo/melodee (`165d177a`) | son émulateur web `web/emu/melodee_web.c` → `webapi_pgi.h` | 8 Mo | non |
| CoralFlower | FicusFicusov/CoralFlower (`46790b9`) | son émulateur web `tools/emu/web/emu_web.c` → `webapi_pgi.h` | 8 Mo | flash.bin |
| Hortator | DeadActive/hortator (`44c9ad4`) | le cœur de son simulateur `tests/sim_core.c` → `webapi_pgi.h` | 8 Mo | flash.bin |
| X0X | charlesvestal/fm1-x0x (`6515e77`) | `firmware/src/app/plat.h` réalisée comme FM1 Tonic (`x0x/plat_x0x.c`) | 2 Mo | fichiers |

- Construire : `guest/felucca_build.py <nom>` (sloop, orbit, karma, jangada, skiff, melodee, coralflower, hortator) ;
  X0X : `guest/x0x/build_x0x.py`. Les dépôts sont clonés dans `~/src`, à leur commit, jamais modifiés.
- Les harnais de test et émulateurs supposent un PC : `sdk/hostshim/` remplace stdio, stdlib, pthread… par des
  bouchons (ce qu'ils écrivent dans des fichiers ne sert pas ici), `sdk/hostlibc.c`, `sdk/libm_lite.c` (sin, exp, pow…
  en double), `sdk/cxxrt.cc` (le peu de C++ de KARMA), `sdk/rtconv.c` (conversions 64 bits).
- `felucca_pgi.h` : le son par `mix_block` sous l'interruption audio, l'interface dans la boucle (16 ms).
  `webapi_pgi.h` : l'émulateur web avance dans la boucle pour garder ~35 ms de son d'avance (une petite file vers
  l'interruption audio), au plus 256 trames par passage pour ne jamais bloquer les commandes.
- Grande zone (Avrek OS 0.6.0) : 8 Mo à 0x81800000 (`PGI_SDRAM8_BASE`, `sdk/sdram8.ld`) pour ceux qui embarquent
  une flash simulée de 1 Mo.
- Sur la carte : `/Guests/<NOM>.pgi` et `/Guests/<NOM>/map.json` (matrice de la page de câblage ; défaut :
  `docs/correspondance/<nom>.json`, clavier façon piano en bas de la grille 16×8, boutons en haut).
- Test : `FEL=<nom> scripts/test.sh vdev_own_felucca` (X0X : `FEL=x0x FEL_NAME=X0X FEL_SOUND_PAD=6,0
  FEL_PGI=guest/x0x/x0x.pgi`, Hortator : `FEL_SOUND_PAD=play`). Les 9 passent au virtuel ; rien n'est encore essayé
  sur la machine (charge processeur inconnue : tout sur un cœur).
