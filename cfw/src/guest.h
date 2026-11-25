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
/* image d'un invité dans la RAM_G de l'EVE (guest_load.c host_screen, guest_screen.c) : au-dessus des
 * caches de widgets de Polyend (0x7a120..0xde120, 50 x 0x2000), zone vue libre sur le Play+ virtuel */
#define GUEST_FB_RAMG 0xE2000u
#define GUEST_FB_W 240u
#define GUEST_FB_H 240u
#define BTN_ENCODER 6     /* clic de l'encodeur sous l'écran */

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
    u32 frames;          /* images dessinées par l'invité (guest_screen.c) */
    u32 last_frame_ms;   /* fw_millis de la dernière image : cadence limitée */
    u32 redraw;          /* 1 : à la sortie, faire recomposer son image à Polyend */
    u32 pads[5];         /* pads tenus (160 bits, index = rangée x 20 + colonne) */
    u32 led_saved;       /* copie de la trame LED de Polyend (tas, 7728 o), faite à l'entrée */
    u32 led_restore;     /* 1 : à la sortie, remettre la trame de Polyend (guest_led.c) */
    u32 led_dirty;       /* 1 : les pads de l'invité sont à repeindre */
    u32 led_ms;          /* fw_millis du dernier pas de la tête de lecture de test */
    u32 led_col;         /* colonne de la tête de lecture de test */
    u32 pgi;             /* 1 : un invité PGI est chargé en OCRAM et prend son, entrées, LED (guest_load.c) */
    u32 picking;         /* 1 : sélecteur d'invités affiché (guest_pick.c) */
    u32 pick;            /* struct pick * sur le tas (liste des invités de /Guests), alloué une fois */
    u32 pick_err;        /* 1 : le dernier invité choisi a été refusé */
    u32 pgi_base;        /* adresse de l'invité chargé (PGI_BASE ou PGI_SDRAM_BASE) */
    u32 bitmap;          /* 1 : l'invité a envoyé son image (pgi_host.screen) : afficher elle, pas la mire */
};
/* 35 mots = 140 octets, à cfw_ram + DIAG_GUEST_OFFSET (voir diag.h) */

int guest_active(void);
int guest_drawing(void);                 /* actif ET menu refermé : l'écran est à l'invité */
volatile struct guest_state *guest_state(void);
void guest_enter(void);                  /* depuis le menu : active, puis referme le menu */
void guest_loop(void);                   /* à chaque tour de boucle principale (loop.c) */
void guest_render(s16 *l, s16 *r);
struct pgi_header;
int guest_load(const char *path);        /* guest_load.c : charge un fichier PGI, 1 si invité prêt */
void guest_unload(void);                 /* l'invité ne reçoit plus rien (retour au sélecteur) */
int guest_pick_enter(void);              /* guest_pick.c : liste /Guests ; 1 si au moins un invité */
void guest_pick_event(u32 kind, u32 a, u32 b);
void guest_pick_leds(void);
const char *guest_pick_name(u32 i);      /* nom affichable du i-ème invité, ou 0 */
u32 guest_pick_count(void);
u32 guest_pick_cursor(void);
const struct pgi_header *guest_pgi(void); /* invité chargé, ou 0 (mire de test intégrée) */
void guest_led_enter(void);              /* guest_led.c */
void guest_led_exit(void);
void guest_led_loop(void);       /* sous interruption audio, à la place du rendu Polyend */

#endif
