/* Avrek OS : état partagé entre own.c (la boucle) et own_guest.c (l'invité chargé depuis la carte).
 * L'état vit sur le tas ; son adresse est dans cfw_ram (voir own.c : validée avant tout usage). */
#ifndef OWN_H
#define OWN_H
#include "cfw.h"
#include "pgi.h"

#define OWN_MAGIC 0x4F574E31u           /* "1NWO" */
#define NVOICE 4
#define TAKE_MS 3000                    /* prise de main au plus tôt 3 s après le démarrage */
#define FIRST_FRAME_MS 3000             /* pas d'image à nous en 3 s : on rend la main */
#define EXIT_MS 2000                    /* Shift + B6 (clic de l'encodeur) tenus : Play+ d'origine */
#define BTN_EXIT ((1u << 10) | (1u << 6))  /* B7 est PLAY dans la matrice de LOOM, B6 y est libre */
#define BTN_POWER 12                    /* interrupteur d'alimentation (arrière) */
#define POWER_MS 1500                   /* tenu : extinction, comme l'officiel */
#define RETRY_MS 2000                   /* pas d'invité : on réessaie de le charger */
#define STOCK_MAGIC 0x53544F43u         /* « redémarrer en Play+ d'origine » : cfw_ram mots 1 et 2 */
#define FRAME_MS 16                     /* ~60 images/s au plus, et seulement quand quelque chose change */
#define DM_STATE 0x14
#define DM_READY 0x34

struct voice {                          /* partagée avec l'interruption audio : tout en volatile */
    volatile u32 inc;                   /* pas de phase (32 bits = un cycle) ; 0 = libre */
    volatile u32 phase;
    volatile s32 gain;                  /* 0..4096, rampe pour éviter les clics */
    volatile u32 on;                    /* 1 tenue, 0 relâchée (le gain redescend) */
    volatile u32 pad;                   /* pad qui la joue */
};

struct own {
    u32 magic;
    u32 active;                         /* 1 : la main est prise */
    void *self;                         /* = l'adresse de la structure (validation) */
    u32 t0;                             /* fw_millis à la prise de main */
    u32 pads[5];                        /* 160 bits : pads tenus */
    s32 knob[15];                       /* valeur cumulée de chaque potard */
    s32 enc;                            /* encodeur cumulé */
    u32 buttons;                        /* boutons tenus (bit = numéro) */
    u32 last_ev;                        /* dernier événement : genre << 24 | a << 8 | état */
    s32 last_d;                         /* son delta (potards, encodeur) */
    u32 dirty;                          /* écran à redessiner */
    u32 led_dirty;
    u32 ev_ms;                          /* heure du plus ancien événement pas encore affiché, + 1 */
    u32 lat_last, lat_max, lat_sum, lat_n;
    u32 loops, loops_ps, sec_ms;
    u32 frame_ms, frames;
    u32 renders;
    u32 exit_ms;                        /* début de l'appui Shift + B7, + 1 */
    u32 give_up;                        /* 1 : pas d'image à nous, rendre la main */
    u32 redraw;                         /* après la sortie : recomposer l'image de Polyend */
    const void *orig_vt;                /* table de listener et rendu d'origine, remis à la sortie */
    u32 (*orig_render)(u32, u32, u32, u32);
    struct voice v[NVOICE];
    /* invité (own_guest.c) */
    u32 pgi;                            /* adresse de l'en-tête de l'invité chargé et initialisé, 0 sinon */
    u32 bitmap;                         /* l'image de l'invité est en RAM_G : on l'affiche */
    u32 cpu_sum, cpu_n, cpu_peak;       /* cycles du calcul audio de l'invité (fenêtre de ~1 s) */
    u32 cpu_avg_done, cpu_peak_done;    /* dernière fenêtre publiée, en cycles */
    u32 bands_ms;                       /* dernier rafraîchissement des bandes latérales */
    const char *msg;                    /* message de l'écran de test (chargement refusé...) */
    u32 try_ms;                         /* dernier essai de chargement de l'invité */
    u32 power_ms;                       /* début de l'appui sur l'interrupteur, + 1 */
    u32 ready;                          /* init de l'invité finie : l'audio peut l'appeler */
};

int own_ram_ok(u32 a);
struct own *own(void);                  /* l'état, ou 0 s'il n'a pas été créé par CE démarrage */

/* own_guest.c */
int og_load(struct own *s);             /* boucle : charge et initialise l'invité de la carte ; 1 si oui */
const struct pgi_header *og_pgi(struct own *s);
void og_event(struct own *s, u32 kind, u32 a, u32 b);
void og_tick(struct own *s);
void og_audio(struct own *s, s16 *l, s16 *r);   /* sous interruption */
void og_draw(struct own *s);            /* compose et envoie l'image (invité + bandes) */
#endif
