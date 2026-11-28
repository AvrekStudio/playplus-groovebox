/* À inclure par un invité. PGI_GUEST(...) déclare l'en-tête attendu par l'hôte, en tête de l'image. */
#ifndef PGI_GUEST_H
#define PGI_GUEST_H

#include "pgi.h"

extern char __pgi_image_end[], __pgi_bss_end[];   /* fournis par pgi.ld */

#define PGI_GUEST(nm, init_fn, audio_fn, event_fn, tick_fn)                                       \
    PGI_GUEST_BANDS(nm, init_fn, audio_fn, event_fn, tick_fn, 0)

/* avec des textes de bandes latérales (ABI 4) : bands = tableau de 16 chaînes (voir pgi.h) */
#define PGI_GUEST_BANDS(nm, init_fn, audio_fn, event_fn, tick_fn, bands_arr)                      \
    __attribute__((section(".pgi_header"), used)) const struct pgi_header pgi_header = {        \
        PGI_MAGIC, PGI_ABI, (unsigned int)__pgi_image_end, (unsigned int)__pgi_bss_end, nm,     \
        init_fn, audio_fn, event_fn, tick_fn, PGI_GUEST_BASE, bands_arr}

#ifndef PGI_GUEST_BASE
#define PGI_GUEST_BASE PGI_BASE    /* -DPGI_GUEST_BASE=PGI_SDRAM_BASE pour un invité lié en SDRAM */
#endif

#endif

/* (ABI 7) avec la mise en page à 5 colonnes dessinée par l'hôte (struct pgi_ui, voir pgi.h) */
#define PGI_GUEST_UI(nm, init_fn, audio_fn, event_fn, tick_fn, bands_arr, ui_ptr)                     \
    __attribute__((section(".pgi_header"), used)) const struct pgi_header pgi_header = {        \
        PGI_MAGIC, PGI_ABI, (unsigned int)__pgi_image_end, (unsigned int)__pgi_bss_end, nm,     \
        init_fn, audio_fn, event_fn, tick_fn, PGI_GUEST_BASE, bands_arr, ui_ptr}

