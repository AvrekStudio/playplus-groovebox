/* Mode invité : notre code prend la main sur le son, les entrées et (bientôt) l'écran, pendant que le
 * firmware Polyend continue de faire tourner les pilotes (pads, potards, LED, écran, carte SD).
 * Base du portage de firmwares tiers (LOOM-8 / fm1-base) : voir docs/analyses/loom-portage.md. */
#ifndef GUEST_H
#define GUEST_H

#include "cfw.h"

/* types d'événements d'entrée, dans l'ordre des slots du listener de l'officiel (vtable 0x6007b27c) */
enum { GEV_PAD, GEV_BUTTON, GEV_KNOB, GEV_TOUCH, GEV_ENCODER, NGEV };

#define GUEST_MAGIC 0x41564B47u        /* "GKVA" : guest_state initialisé */
#define BTN_BACK 7
#define BTN_SHIFT 10

struct guest_state {
    u32 magic;
    u32 active;          /* 1 : mode invité */
    u32 close_menu;      /* passes de boucle restantes pour refermer le menu après l'activation */
    u32 shift;           /* Shift tenu (suivi même en mode invité, pour la sortie Shift + Back) */
    u32 phase;           /* oscillateur de test */
    u32 ev_count[NGEV];  /* événements reçus en mode invité, par type */
    u32 last[3];         /* dernier événement : type, a, b */
    u32 renders;         /* blocs audio rendus par l'invité */
    u32 held;            /* boutons appuyés vus par l'invité (bit = id) : on ignore le relâché d'un
                          * bouton appuyé avant l'entrée (ex. la touche qui a validé « Mode invite ») */
};
/* 16 mots = 64 octets, à cfw_ram + DIAG_GUEST_OFFSET (voir diag.h) */

int guest_active(void);
void guest_enter(void);                  /* depuis le menu : active, puis referme le menu */
void guest_loop(void);                   /* à chaque tour de boucle principale (loop.c) */
void guest_render(s16 *l, s16 *r);       /* sous interruption audio, à la place du rendu Polyend */

#endif
