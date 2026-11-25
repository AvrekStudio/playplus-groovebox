/* Mode invité (voir guest.h).
 *
 * ENTRÉES. Crochets (target.json, « input listener vtable [0..4] ») : l'objet listener de l'officiel
 * (app + 0x1c = 0x20018c34) a une vtable en flash à 0x6007b27c ; ses 5 premiers slots reçoivent les
 * événements déjà décodés par la lecture du matériel (0x600aab28, boucle principale) :
 *   [0] pad (index = rangée x 20 + colonne, état), [1] bouton (id, état), [2] potard (index, delta),
 *   [3] toucher de potard (index, état), [4] encodeur (0, delta).  États : 0 appui, 1 relâché, 2 tenu.
 * Nos cfw_in_* remplacent ces 5 slots. Hors mode invité ils appellent l'original (fw_in_*, mêmes
 * adresses que les valeurs attendues par les crochets). En mode invité ils gardent l'événement pour
 * l'invité et ne le transmettent PAS à Polyend, sauf la sortie : Shift (10) tenu + Back (7).
 *
 * SON. audio_load.c appelle guest_render() à la place du callback de rendu d'origine quand le mode
 * invité est actif. Format, vérifié dans la routine d'IRQ audio 0x13a18 : deux tampons int16 de
 * 128 échantillons (gauche, droite), que l'IRQ entrelace ensuite dans fw_audio_out. Pour l'instant :
 * un son de test (triangle à 220 Hz, discret).
 *
 * MENU. guest_enter() est appelée par l'action « Mode invite » du menu (menu.c). Le menu est encore
 * ouvert à ce moment-là : guest_loop() le referme en rejouant des appuis sur Back à l'original,
 * jusqu'à ce que l'écran courant ne soit plus le menu (UI manager = *(listener + 8),
 * écran courant en +UI_CUR_SCREEN, écran du menu en +UI_MENU_SCREEN). */
#include "cfw.h"
#include "target.h"
#include "diag.h"
#include "guest.h"

extern void fw_in_pad(void *l, u32 a, u32 b, u32 c);
extern void fw_in_button(void *l, u32 a, u32 b, u32 c);
extern void fw_in_knob(void *l, u32 a, u32 b, u32 c);
extern void fw_in_track(void *l, u32 a, u32 b, u32 c);
extern void fw_in_encoder(void *l, u32 a, u32 b, u32 c);
extern u8 cfw_ram[];

extern char fw_input_listener[];          /* 0x20018c34 = app (0x20018c18) + 0x1c (symbols.ld) */
#define LISTENER ((void *)fw_input_listener)

static volatile struct guest_state *gs(void)
{
    volatile struct guest_state *g = (volatile struct guest_state *)(cfw_ram + DIAG_GUEST_OFFSET);
    if (g->magic != GUEST_MAGIC) {
        u32 i;
        for (i = 0; i < sizeof(struct guest_state) / 4; i++)
            ((volatile u32 *)g)[i] = 0;
        g->magic = GUEST_MAGIC;
    }
    return g;
}

int guest_active(void)
{
    const volatile struct guest_state *g = (const volatile struct guest_state *)(cfw_ram + DIAG_GUEST_OFFSET);
    return g->magic == GUEST_MAGIC && g->active;
}

void guest_enter(void)
{
    volatile struct guest_state *g = gs();
    u32 i;
    for (i = 0; i < NGEV; i++)
        g->ev_count[i] = 0;
    g->renders = 0;
    g->held = 0;
    g->close_menu = 8;   /* au plus 8 Back : un par tour de boucle, jusqu'à quitter le menu */
    g->active = 1;
}

static int menu_open(void)
{
    char *ui = (char *)PTR(LISTENER, 8);
    return ui && PTR(ui, UI_CUR_SCREEN) == PTR(ui, UI_MENU_SCREEN);
}

void guest_loop(void)
{
    volatile struct guest_state *g = gs();
    if (g->close_menu) {
        g->close_menu--;
        if (menu_open()) {
            fw_in_button(LISTENER, BTN_BACK, 0, 0);
            fw_in_button(LISTENER, BTN_BACK, 1, 0);
        } else {
            g->close_menu = 0;
        }
    }
}

/* Vrai si l'événement est pris par l'invité. Shift est suivi dans tous les cas. */
static int take(u32 kind, u32 a, u32 b)
{
    volatile struct guest_state *g = gs();
    if (kind == GEV_BUTTON && a == BTN_SHIFT)
        g->shift = (b != 1);
    if (!g->active || g->close_menu)
        return 0;
    if (kind == GEV_BUTTON && a == BTN_BACK && b == 0 && g->shift) {
        g->active = 0;               /* sortie : Shift + Back. Polyend reprend la main. */
        return 1;
    }
    if (kind == GEV_BUTTON && a < 32) {
        u32 bit = 1u << a;
        if (b == 0)
            g->held |= bit;
        else if (!(g->held & bit))
            return 1;                /* relâché/tenu d'un appui d'avant l'entrée : avalé, non compté */
        else if (b == 1)
            g->held &= ~bit;
    }
    g->ev_count[kind]++;
    g->last[0] = kind;
    g->last[1] = a;
    g->last[2] = b;
    return 1;
}

void cfw_in_pad(void *l, u32 a, u32 b, u32 c)     { if (!take(GEV_PAD, a, b)) fw_in_pad(l, a, b, c); }
void cfw_in_button(void *l, u32 a, u32 b, u32 c)  { if (!take(GEV_BUTTON, a, b)) fw_in_button(l, a, b, c); }
void cfw_in_knob(void *l, u32 a, u32 b, u32 c)    { if (!take(GEV_KNOB, a, b)) fw_in_knob(l, a, b, c); }
void cfw_in_track(void *l, u32 a, u32 b, u32 c)   { if (!take(GEV_TOUCH, a, b)) fw_in_track(l, a, b, c); }
void cfw_in_encoder(void *l, u32 a, u32 b, u32 c) { if (!take(GEV_ENCODER, a, b)) fw_in_encoder(l, a, b, c); }

/* Son de test : triangle à 220 Hz, amplitude 1/8 de la pleine échelle. Incrément de phase =
 * 2^32 x 220 / 44100 = 21426069. Sous interruption : arithmétique entière seulement. */
void guest_render(s16 *l, s16 *r)
{
    volatile struct guest_state *g = (volatile struct guest_state *)(cfw_ram + DIAG_GUEST_OFFSET);
    u32 ph = g->phase, i;
    for (i = 0; i < 128; i++) {
        s32 tri = (s32)(ph >> 16) - 32768;            /* dent de scie -32768..32767 */
        tri = (tri < 0 ? -tri : tri) * 2 - 32768;      /* triangle */
        l[i] = r[i] = (s16)(tri >> 3);
        ph += 21426069u;
    }
    g->phase = ph;
    g->renders++;
}
