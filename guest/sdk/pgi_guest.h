/* À inclure par un invité. PGI_GUEST(...) déclare l'en-tête attendu par l'hôte, en tête de l'image. */
#ifndef PGI_GUEST_H
#define PGI_GUEST_H

#include "pgi.h"

extern char __pgi_image_end[], __pgi_bss_end[];   /* fournis par pgi.ld */

#define PGI_GUEST(nm, init_fn, audio_fn, event_fn, tick_fn)                                       \
    __attribute__((section(".pgi_header"), used)) const struct pgi_header pgi_header = {        \
        PGI_MAGIC, PGI_ABI, (unsigned int)__pgi_image_end, (unsigned int)__pgi_bss_end, nm,     \
        init_fn, audio_fn, event_fn, tick_fn}

#endif
