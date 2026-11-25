/* Mode invité : l'écran (jalon M2).
 *
 * COMMENT L'OFFICIEL DESSINE (vérifié au désassembleur et sur le Play+ virtuel, 25/11/2026)
 *   La boucle de lecture du matériel (0x600aab28) appelle, à l'instruction 0x600aab34,
 *   « BL 0x600ab394 » = mise à jour du gestionnaire d'affichage (dm = objet matériel + 4).
 *   0x600ab394 ne fait rien tant que dm+0x34 != 1 ou dm+4 == 0 (écran pas prêt), sinon il fait avancer
 *   un automate dont l'état est l'octet dm+0x14 :
 *     0 : cherche un widget à redessiner ;
 *     1 : redessine UN widget dans son cache en RAM_G de l'EVE (un emplacement de 0x2000 o par widget) ;
 *     2 : compose l'image complète (0x600ab290..0x600ab30a) : CMD_DLSTART, effacement, puis pour
 *         chaque widget visible un ajout de son cache, DISPLAY, CMD_SWAP.
 *   L'image n'est donc recomposée que quand quelque chose change (0 image/s au repos, mesuré).
 *
 * CE QU'ON FAIT
 *   Crochet « bl » (target.json) sur 0x600aab34 : cfw_display_update remplace l'appel.
 *   - Hors mode invité : on appelle l'original (fw_display_update). Juste après une sortie du mode
 *     invité, on met d'abord l'automate à l'état 2 (quand il est au repos) : Polyend recompose son image
 *     à partir de ses caches, intacts puisqu'on n'écrit jamais en RAM_G.
 *   - En mode invité (menu refermé) : on n'appelle PAS l'original (Polyend ne dessine plus) et on
 *     envoie notre propre image, une liste d'affichage complète dans l'anneau de commandes (RAM_CMD),
 *     au plus toutes les FRAME_MS, et seulement quand le coprocesseur a fini la précédente.
 *
 * L'ANNEAU DE COMMANDES fait 4 Ko. On n'envoie que quand il est vide (fw_eve_idle), et une image fait
 * au plus ~750 mots (< 1023) : pas de débordement. Quand l'offset d'écriture revient à 0, on ferme et
 * on rouvre la rafale pour que l'adresse SPI reparte du début de l'anneau.
 *
 * Pour l'instant l'image est une mire de test à nous : la grille des 160 pads (allumés quand ils sont
 * tenus), une barre animée, les compteurs d'événements. Elle prouve qu'on tient tout l'écran ; le rendu
 * d'un firmware invité (LOOM, 240 x 240) viendra à sa place. */
#include "cfw.h"
#include "target.h"
#include "fmt.h"
#include "guest.h"
#include "pgi.h"

extern void fw_display_update(void *dm);
extern void fw_eve_begin(void);
extern void fw_eve_end(void);
extern void fw_eve_word(u32 w);
extern int fw_eve_idle(void);
extern void fw_eve_text(u32 x, s32 y, u32 font, u32 opts, const char *s, u32 maxlen);
extern volatile u16 fw_eve_wp;
extern volatile u32 fw_millis;

#define DM_STATE 0x14     /* u8 : état de l'automate (0 repos, 1 widget, 2 composition) */
#define DM_READY 0x34     /* u32 : 1 quand l'écran est initialisé */
#define FRAME_MS 33       /* ~30 images/s */

/* mots de liste d'affichage FT81x */
#define CMD_DLSTART 0xffffff00u
#define CMD_SWAP 0xffffff01u
#define DL_DISPLAY 0x00000000u
#define DL_CLEAR_RGB(c) (0x02000000u | ((c) & 0xffffff))
#define DL_CLEAR_ALL 0x26000007u
#define DL_RGB(c) (0x04000000u | ((c) & 0xffffff))
#define DL_BEGIN_RECTS 0x1f000009u
#define DL_END 0x21000000u
#define DL_VFORMAT_1PX 0x27000000u   /* VERTEX_FORMAT(0) : coordonnées en pixels entiers */
#define DL_V(x, y) (0x40000000u | (((u32)(x) & 0x7fff) << 15) | ((u32)(y) & 0x7fff))
#define FONT_SMALL 8      /* polices chargées par l'officiel (CMD_SETFONT2, poignées 8..12) */
#define FONT_LARGE 12
#define OPT_CENTERX 512

/* palette de la mire (à nous) */
#define C_BG 0x101418
#define C_PAD 0x262c34
#define C_LIT 0xffb000
#define C_BAR 0x40c0ff
#define C_TEXT 0xe8e8e8
#define C_DIM 0x8090a0

/* grille des pads : 20 x 8 cases de 15 px (13 px + 2 px d'espace), centrée en largeur */
#define GRID_X 10
#define GRID_Y 56
#define CELL 15

static void put(u32 w)
{
    fw_eve_word(w);
    fw_eve_wp = (u16)((fw_eve_wp + 4) & 0xfff);
    if (fw_eve_wp == 0) {   /* fin de l'anneau : rafale suivante à l'adresse RAM_CMD + 0 */
        fw_eve_end();
        fw_eve_begin();
    }
}

static void rect(int x0, int y0, int x1, int y1)
{
    put(DL_V(x0, y0));
    put(DL_V(x1, y1));
}

/* fw_eve_text écrit la commande et la chaîne, et fait avancer fw_eve_wp lui-même. Il ne sait pas
 * couper à la fin de l'anneau : on ferme la rafale avant s'il ne reste pas la place. */
static void text(int x, int y, u32 font, u32 color, u32 opts, const char *s)
{
    u32 n = 0;
    put(DL_RGB(color));
    while (s[n])
        n++;
    if (fw_eve_wp + 16 + n + 4 >= 0x1000) {
        while (fw_eve_wp)
            put(DL_RGB(color));       /* bourrage jusqu'à la fin de l'anneau */
    }
    fw_eve_text((u32)x & 0xffff, y, font, opts, s, 0);
}

static int pad_lit(volatile struct guest_state *g, u32 i)
{
    return (g->pads[i >> 5] >> (i & 31)) & 1;
}

static void draw_frame(volatile struct guest_state *g)
{
    char buf[64], *p;
    u32 i, t = g->frames;
    int bx;

    fw_eve_begin();
    put(CMD_DLSTART);
    put(DL_CLEAR_RGB(C_BG));
    put(DL_CLEAR_ALL);
    put(DL_VFORMAT_1PX);

    if (guest_pgi()) {                /* nom de l'invité chargé (borné à 15 caractères) */
        char nm[16];
        for (i = 0; i < 15 && guest_pgi()->name[i]; i++)
            nm[i] = guest_pgi()->name[i];
        nm[i] = 0;
        text(160, 8, FONT_LARGE, C_TEXT, OPT_CENTERX, nm);
    } else {
        text(160, 8, FONT_LARGE, C_TEXT, OPT_CENTERX, "MODE INVITE");
    }

    /* grille : toutes les cases éteintes, puis les cases allumées par-dessus (deux couleurs seulement) */
    put(DL_BEGIN_RECTS);
    put(DL_RGB(C_PAD));
    for (i = 0; i < 160; i++) {
        int x = GRID_X + (int)(i % 20) * CELL, y = GRID_Y + (int)(i / 20) * CELL;
        rect(x, y, x + CELL - 3, y + CELL - 3);
    }
    put(DL_RGB(C_LIT));
    for (i = 0; i < 160; i++) {
        if (pad_lit(g, i)) {
            int x = GRID_X + (int)(i % 20) * CELL, y = GRID_Y + (int)(i / 20) * CELL;
            rect(x, y, x + CELL - 3, y + CELL - 3);
        }
    }
    /* barre animée : va-et-vient sur la largeur de la grille, une période de 120 images (~4 s) */
    bx = (int)(t % 120);
    if (bx >= 60)
        bx = 120 - bx;
    bx = GRID_X + bx * (20 * CELL - 40) / 60;
    put(DL_RGB(C_BAR));
    rect(bx, 186, bx + 40, 192);
    put(DL_END);

    p = fmt_str(buf, "pads ");
    p = fmt_uint(p, g->ev_count[GEV_PAD]);
    p = fmt_str(p, "  boutons ");
    p = fmt_uint(p, g->ev_count[GEV_BUTTON]);
    p = fmt_str(p, "  potards ");
    p = fmt_uint(p, g->ev_count[GEV_KNOB]);
    *p = 0;
    text(160, 200, FONT_SMALL, C_DIM, OPT_CENTERX, buf);
    text(160, 220, FONT_SMALL, C_DIM, OPT_CENTERX, "Shift + Back : sortir");

    put(DL_DISPLAY);
    put(CMD_SWAP);
    fw_eve_end();
    g->frames = t + 1;
}

void cfw_display_update(void *dm)
{
    volatile struct guest_state *g = guest_state();

    if (!guest_drawing()) {
        if (g->redraw && U8(dm, DM_STATE) == 0) {
            U8(dm, DM_STATE) = 2;      /* recomposer l'image de Polyend à partir de ses caches */
            g->redraw = 0;
        }
        fw_display_update(dm);
        return;
    }
    if (U32(dm, DM_READY) != 1 || !PTR(dm, 4))
        return;                        /* écran pas initialisé (même garde que l'original) */
    if (fw_millis - g->last_frame_ms < FRAME_MS || !fw_eve_idle())
        return;
    g->last_frame_ms = fw_millis;
    draw_frame(g);
}
