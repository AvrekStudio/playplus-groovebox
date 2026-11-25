/* Diagnostic : état partagé entre la mesure (audio_load.c, loop.c) et l'affichage.
 *
 * Carte de cfw_ram (256 octets libres à 0x20002700, voir symbols.ld). À tenir à jour ici ET dans
 * docs/JOURNAL.md à chaque ajout :
 *   0x00..0x0F  struct loop_state   (loop.c)
 *   0x10..0x3F  struct audio_state  (audio_load.c)
 *   0x40..0xB3  struct guest_state  (guest.c)
 *   0xB4..0xFF  libre */
#ifndef DIAG_H
#define DIAG_H

#include "cfw.h"

#define DIAG_LOOP_OFFSET 0x00
#define DIAG_AUDIO_OFFSET 0x10
#define DIAG_GUEST_OFFSET 0x40

#define DIAG_AUDIO_MAGIC 0x41564B41u   /* "AKVA" : audio_state initialisé */
#define DIAG_WINDOW_BLOCKS 344u        /* blocs par fenêtre de moyenne : 344 x 2,902 ms = 1 s */
#define DIAG_BLOCK_BUDGET 1741497u     /* cycles disponibles par bloc : 600 MHz x 128 / 44100 */

struct audio_state {
    u32 magic;
    u32 orig;        /* callback de rendu d'origine (adresse Thumb) */
    u32 installs;    /* nombre de fois où on a (re)pris le pointeur */
    u32 blocks;      /* blocs calculés depuis le démarrage */
    u32 last;        /* cycles du dernier bloc */
    u32 peak_all;    /* pire bloc depuis le démarrage */
    u32 win_sum;     /* fenêtre en cours : somme des cycles */
    u32 win_peak;    /*                    pire bloc */
    u32 win_n;       /*                    blocs comptés */
    u32 avg_done;    /* dernière fenêtre complète : cycles moyens par bloc */
    u32 peak_done;   /*                             pire bloc */
    u32 windows;     /* fenêtres complètes depuis le démarrage */
};
/* 12 mots = 48 octets : 0x10..0x3F */

void audio_load_install(void);

#endif
