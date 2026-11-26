/* Prise de contrôle du Play+ : notre boucle remplace celle de l'officiel (étape 2, boucle de test).
 *
 * POURQUOI ET COMMENT (docs/analyses/prise-de-controle.md)
 *   L'officiel démarre normalement (horloges, SDRAM, carte SD, écran, codec, pilotes, application),
 *   puis entre dans sa boucle principale en 0x600ad852. Son premier appel, en 0x600ad854, vise une
 *   fonction vide (0x600aba50) : crochet « bl » vers cfw_own_main, qui ne revient jamais. À partir de là :
 *   - plus d'application Polyend (interface, séquenceur, sauvegardes) : on n'appelle plus la case 1 de
 *     sa table. fw_bg_services (0x600b0f90) EST appelé : c'est lui qui interroge les deux contrôleurs
 *     des PADS (I2C, adresse 0x28 sur LPI2C1 et LPI2C2, via 0x600aef60 → 0x600ad9bb → 0x600af5c0 ;
 *     relevé au virtuel le 26/11 après 0.1.1 : sans lui, aucun appui de pad n'arrive). 0x600aacf8 n'est
 *     pas appelé (rôle inconnu, USB / MIDI laissés de côté pour l'instant) ;
 *   - CONTRÔLES : on garde le balayage matériel fw_hw_update(fw_hw) ; on remplace le pointeur de table
 *     du listener [fw_input_listener] (0x6007b27c d'origine) par own_listener_vt : tous les événements
 *     arrivent ici. Le TOUCHER des potards (case 3) est ignoré, sur demande du propriétaire ;
 *   - ÉCRAN : fw_hw_update appelle l'automate d'affichage de Polyend en 0x600aab34 ; crochet « bl » vers
 *     cfw_own_display, qui n'appelle plus l'original une fois la main prise et envoie notre image
 *     (liste d'affichage complète dans l'anneau de commandes, comme guest_screen.c) ;
 *   - LED DES PADS : on encode nous-mêmes (fw_led_encode) ; les deux chemins d'écriture de Polyend sont
 *     bloqués (cfw_own_led_set, cfw_own_led_blink, mêmes crochets que guest_led.c) ;
 *   - SON : le pointeur fw_audio_render_cb (0x2000301c) désigne own_render ; plus aucun calcul audio de
 *     Polyend. own_render tourne SOUS INTERRUPTION : rapide, sans appel ni accès fichier.
 *
 * INVITÉ (own_guest.c) : à la prise de main, on charge le firmware FM-1 de la carte (LOOM-8) ; s'il manque,
 * on réessaie toutes les RETRY_MS et l'écran de test reste. Avec un invité : contrôles, son, image et
 * tâche de fond vont à lui (le toucher des potards jamais ; l'interrupteur jamais).
 * INTERRUPTEUR (bouton 12) tenu POWER_MS : fw_power_off, comme l'officiel (sans sauvegarde de Polyend).
 * SORTIE DE SECOURS : Shift (B10) + B6 tenus EXIT_MS. Sans invité : on rend la main au Play+ d'origine
 * jusqu'au prochain redémarrage (give_back). Avec invité (il occupe la mémoire des samples de Polyend) :
 * on écrit STOCK_MAGIC dans cfw_ram (qui survit au redémarrage, pas à l'extinction) et on redémarre :
 * l'officiel démarre seul, et ce jusqu'à ce qu'on éteigne la machine. Ancienne sortie (0.1.x) : B7, qui
 * est PLAY dans la matrice de LOOM-8.
 * SORTIE (sans invité) : on rend la main au Play+ d'origine jusqu'au prochain
 * redémarrage (accès à son menu de mise à jour sans le bouton reset). On remet sa table de listener et
 * son rendu audio, puis notre fonction continue en refaisant exactement ce que fait sa boucle
 * (stock_loop : services de fond, matériel, 2e service, application, battement de 500 ms, tâche de 5 s).
 * On ne « revient » jamais de cfw_own_main : le crochet est au début de chaque tour de sa boucle, et
 * rien en RAM ne permet de savoir de façon sûre qu'on a déjà démarré (cfw_ram survit à un reset).
 *
 * LA BOUCLE DE TEST montre que tout répond : pads tenus (écran + LED + une note par pad, 4 voix),
 * valeurs des 15 potards et de l'encodeur, boutons tenus, dernier événement, tours de boucle par
 * seconde et latence événement → image (moyenne, max).
 *
 * État : sur le tas (fw_operator_new, une fois), pointeur dans cfw_ram (cette cible n'a que ce module).
 *
 * PIÈGE VU SUR LA MACHINE (26/11, 0.1.0 figé sur le logo) : cfw_ram n'est PAS remise à zéro au
 * démarrage. Elle contenait les données du firmware d'avant (Knob Touch : son nombre magique en tête),
 * et nos crochets LED, appelés pendant le démarrage, s'en servaient comme pointeur : faute, blocage.
 * Le virtuel part d'une RAM à zéro et ne le voyait pas. Désormais own() valide tout : adresse en RAM,
 * alignement, nombre magique, auto-référence, et heure de prise de main <= fw_millis (fw_millis repart
 * de 0 à chaque démarrage, et on ne prend la main qu'après TAKE_MS).
 *
 * PRISE DE MAIN DIFFÉRÉE : cfw_own_main est appelée au début de CHAQUE tour de la boucle de l'officiel.
 * Tant que fw_millis < TAKE_MS, ou que l'automate d'affichage de Polyend n'est pas au repos, ou que le
 * coprocesseur de l'écran est occupé, on revient tout de suite : l'officiel finit son démarrage et son
 * premier écran. Ensuite on prend la main pour de bon. Si notre première image ne part pas dans les
 * FIRST_FRAME_MS suivantes, on rend la main au Play+ d'origine (give_back). */
#include "cfw.h"
#include "target.h"
#include "fmt.h"

extern void fw_hw_update(void *hw);
extern char fw_hw[];
extern char fw_input_listener[];
extern void fw_display_update(void *dm);
extern void fw_eve_begin(void);
extern void fw_eve_end(void);
extern void fw_eve_word(u32 w);
extern int fw_eve_idle(void);
extern void fw_eve_text(u32 x, s32 y, u32 font, u32 opts, const char *s, u32 maxlen);
extern volatile u16 fw_eve_wp;
extern char fw_pad_leds[];
extern void fw_led_encode(void *led, u32 i, u32 r, u32 g, u32 b);
extern void fw_pad_led_set(void *mgr, u32 i, const u8 *c, const u8 *c2, u32 mode);
extern void *fw_operator_new(u32 size);
extern u32 (*volatile fw_audio_render_cb)(u32, u32, u32, u32);
extern volatile u32 fw_millis;
extern u8 cfw_ram[];
extern void fw_bg_services(void);
extern void fw_bg_service2(void *o);
extern char fw_bg_service2_obj[];
extern void *volatile fw_app_holder;
extern void fw_gpio_write(void *port, u32 pin, u32 val);
extern void fw_periodic_5s(void *o);
extern void fw_power_off(void *hw);
extern char fw_periodic_5s_obj[];

#include "own.h"

int own_ram_ok(u32 a)
{
    return (a & 3) == 0 && ((a >= 0x20000000u && a < 0x20080000u) || (a >= 0x20200000u && a < 0x20280000u) ||
                            (a >= 0x80000000u && a < 0x82000000u));
}

/* L'état, ou 0 s'il n'a pas été créé par CE démarrage (voir le piège ci-dessus). */
struct own *own(void)
{
    struct own *s = *(struct own *volatile *)cfw_ram;
    if (!own_ram_ok((u32)s) || s->magic != OWN_MAGIC || s->self != (void *)s || s->t0 > fw_millis || s->t0 < TAKE_MS)
        return 0;
    return s;
}

/* ---- son (sous interruption) ------------------------------------------------------------------ */

static u32 own_render(u32 a, u32 b, u32 c, u32 d)
{
    s16 *l = (s16 *)a, *r = (s16 *)b;
    struct own *s = own();
    u32 i, k;
    (void)c;
    (void)d;
    for (i = 0; i < 128; i++)
        l[i] = r[i] = 0;
    if (!s)
        return 0;
    if (og_pgi(s)) {                                    /* l'invité calcule le son */
        og_audio(s, l, r);
        s->renders++;
        return 0;
    }
    for (k = 0; k < NVOICE; k++) {
        struct voice *v = &s->v[k];
        u32 ph = v->phase, inc = v->inc;
        s32 g = v->gain, tgt = v->on ? 4096 : 0;
        if (!inc)
            continue;
        for (i = 0; i < 128; i++) {
            s32 tri = (s32)(ph >> 16) - 32768;          /* dent de scie -> triangle */
            tri = (tri < 0 ? -tri : tri) * 2 - 32768;
            if (g < tgt)
                g += 32;                                /* ~3 ms de montée / descente */
            else if (g > tgt)
                g -= 32;
            l[i] = r[i] = (s16)(l[i] + ((tri >> 3) * g >> 12));
            ph += inc;
        }
        v->phase = ph;
        v->gain = g;
        if (!v->on && g <= 0)
            v->inc = 0;                                 /* voix libérée */
    }
    s->renders++;
    return 0;
}

/* pas de phase pour la note n (demi-tons, 60 = Do 4) : table d'une octave puis décalages */
static const u32 OCT4[12] = {   /* round(f * 2^32 / 44100), f de Do 4 à Si 4 */
    25480328u, 26995679u, 28601151u, 30302091u, 32104183u, 34013447u,
    36036255u, 38179358u, 40449917u, 42855501u, 45404143u, 48104354u};

static u32 note_inc(s32 n)
{
    s32 o = n / 12 - 5, k = n % 12;
    u32 inc = OCT4[k];
    if (o > 0)
        inc <<= o;
    else if (o < 0)
        inc >>= -o;
    return inc;
}

/* pad -> note : grille 16x8 = rangées d'une quarte (bas = grave), grille 4x8 = 4 demi-tons par rangée */
static s32 pad_note(u32 i)
{
    u32 row = i / 20, col = i % 20;
    if (col < 16)
        return 48 + (s32)(7 - row) * 5 + (s32)col;
    return 60 + (s32)(7 - row) * 4 + (s32)(col - 16);
}

static void voice_on(struct own *s, u32 pad)
{
    u32 k, best = 0;
    for (k = 0; k < NVOICE; k++)
        if (!s->v[k].inc) {
            best = k;
            goto found;
        }
    for (k = 1; k < NVOICE; k++)                        /* toutes prises : la plus faible */
        if (s->v[k].gain < s->v[best].gain)
            best = k;
found:
    s->v[best].on = 0;
    s->v[best].pad = pad;
    s->v[best].phase = 0;
    s->v[best].gain = 0;
    s->v[best].inc = note_inc(pad_note(pad));
    s->v[best].on = 1;
}

static void voice_off(struct own *s, u32 pad)
{
    u32 k;
    for (k = 0; k < NVOICE; k++)
        if (s->v[k].on && s->v[k].pad == pad)
            s->v[k].on = 0;
}

/* ---- contrôles : notre table de listener (même ordre que 0x6007b27c) ------------------------- */

static void touched(struct own *s, u32 kind, u32 a, u32 st, s32 d)
{
    s->last_ev = kind << 24 | (a & 0xffff) << 8 | (st & 0xff);
    s->last_d = d;
    s->dirty = 1;
    if (!s->ev_ms)
        s->ev_ms = fw_millis + 1;
}

static void ev_pad(void *l, u32 a, u32 b, u32 c)
{
    struct own *s = own();
    (void)l;
    (void)c;
    if (a >= 160)
        return;
    if (og_pgi(s)) {
        og_event(s, PGI_EV_PAD, a, b);
        touched(s, 1, a, b, 0);
        return;
    }
    if (b == 0) {
        s->pads[a >> 5] |= 1u << (a & 31);
        voice_on(s, a);
    } else if (b == 1) {
        s->pads[a >> 5] &= ~(1u << (a & 31));
        voice_off(s, a);
    } else
        return;                                         /* « tenu » : rien de neuf */
    s->led_dirty = 1;
    touched(s, 1, a, b, 0);
}

static void ev_button(void *l, u32 a, u32 b, u32 c)
{
    struct own *s = own();
    (void)l;
    (void)c;
    if (a > 31)
        return;
    if (b == 0)
        s->buttons |= 1u << a;
    else if (b == 1)
        s->buttons &= ~(1u << a);
    else
        return;
    if (og_pgi(s) && a != BTN_POWER)
        og_event(s, PGI_EV_BUTTON, a, b);
    touched(s, 2, a, b, 0);
}

static void ev_knob(void *l, u32 a, u32 b, u32 c)
{
    struct own *s = own();
    (void)l;
    (void)c;
    if (a >= 15)
        return;
    if (og_pgi(s))
        og_event(s, PGI_EV_KNOB, a, b);
    s->knob[a] += (s32)b;
    touched(s, 3, a, 0, (s32)b);
}

static void ev_touch(void *l, u32 a, u32 b, u32 c)
{
    (void)l;                                            /* toucher des potards : ignoré, volontairement */
    (void)a;
    (void)b;
    (void)c;
}

static void ev_encoder(void *l, u32 a, u32 b, u32 c)
{
    struct own *s = own();
    (void)l;
    (void)a;
    (void)c;
    if (og_pgi(s))
        og_event(s, PGI_EV_ENCODER, 0, b);
    s->enc += (s32)b;
    touched(s, 4, 0, 0, (s32)b);
}

typedef void (*ev_fn)(void *, u32, u32, u32);
static const ev_fn own_listener_vt[5] = {ev_pad, ev_button, ev_knob, ev_touch, ev_encoder};

/* ---- LED ---------------------------------------------------------------------------------------- */

void cfw_own_led_set(void *mgr, u32 i, const u8 *c, const u8 *c2, u32 mode)
{
    struct own *s = own();
    if (s && s->active)
        return;                                         /* les pads sont à nous */
    fw_pad_led_set(mgr, i, c, c2, mode);
}

void cfw_own_led_blink(void *led, u32 i, u32 r, u32 g, u32 b)
{
    struct own *s = own();
    if (s && s->active)
        return;
    fw_led_encode(led, i, r, g, b);
}

static void leds(struct own *s)
{
    u32 i;
    if (!s->led_dirty)
        return;
    s->led_dirty = 0;
    for (i = 0; i < 160; i++) {
        if ((s->pads[i >> 5] >> (i & 31)) & 1)
            fw_led_encode(fw_pad_leds, i, 255, 96, 0);  /* tenu : orange */
        else if (i % 20 < 16)
            fw_led_encode(fw_pad_leds, i, 0, 6, 22);    /* grille 16x8 : bleu nuit */
        else
            fw_led_encode(fw_pad_leds, i, 14, 0, 22);   /* grille 4x8 : violet nuit */
    }
}

/* ---- écran (320 x 240) ------------------------------------------------------------------------ */

#define CMD_DLSTART 0xffffff00u
#define CMD_SWAP 0xffffff01u
#define DL_DISPLAY 0x00000000u
#define DL_CLEAR_RGB(c) (0x02000000u | ((c) & 0xffffff))
#define DL_CLEAR_ALL 0x26000007u
#define DL_RGB(c) (0x04000000u | ((c) & 0xffffff))
#define DL_BEGIN_RECTS 0x1f000009u
#define DL_END 0x21000000u
#define DL_VFORMAT_1PX 0x27000000u
#define DL_V(x, y) (0x40000000u | (((u32)(x) & 0x7fff) << 15) | ((u32)(y) & 0x7fff))
#define FONT_SMALL 8
#define FONT_LARGE 12
#define C_BG 0x0e1013
#define C_OFF 0x262a31
#define C_LIT 0xff7a2f
#define C_TEXT 0xe9e9e6
#define C_DIM 0x8a9099
#define C_ACC 0x38c6e8

static void put(u32 w)
{
    fw_eve_word(w);
    fw_eve_wp = (u16)((fw_eve_wp + 4) & 0xfff);
    if (fw_eve_wp == 0) {               /* fin de l'anneau : rafale suivante au début (guest_screen.c) */
        fw_eve_end();
        fw_eve_begin();
    }
}

static void rect(int x0, int y0, int x1, int y1)
{
    put(DL_V(x0, y0));
    put(DL_V(x1, y1));
}

static void text(int x, int y, u32 font, u32 color, const char *str)
{
    u32 n = 0;
    put(DL_RGB(color));
    while (str[n])
        n++;
    if (fw_eve_wp + 16 + n + 4 >= 0x1000) {
        while (fw_eve_wp)
            put(DL_RGB(color));         /* bourrage jusqu'à la fin de l'anneau */
    }
    fw_eve_text((u32)x & 0xffff, y, font, 0, str, 0);
}

static char *fmt_int(char *p, s32 v)
{
    if (v < 0) {
        *p++ = '-';
        v = -v;
    }
    return fmt_uint(p, (u32)v);
}

static void draw(struct own *s)
{
    char buf[64], *p;
    u32 i, k = s->last_ev >> 24, a = (s->last_ev >> 8) & 0xffff, st = s->last_ev & 0xff;

    fw_eve_begin();
    put(CMD_DLSTART);
    put(DL_CLEAR_RGB(C_BG));
    put(DL_CLEAR_ALL);
    put(DL_VFORMAT_1PX);
    text(6, 2, FONT_SMALL, C_ACC, "AVREK - boucle de test");

    put(DL_BEGIN_RECTS);                                /* les 160 pads, 14 px, grille 4x8 décalée */
    for (i = 0; i < 160; i++) {
        int c = (int)(i % 20), x = 8 + c * 15 + (c >= 16 ? 6 : 0), y = 20 + (int)(i / 20) * 13;
        put(DL_RGB(((s->pads[i >> 5] >> (i & 31)) & 1) ? C_LIT : C_OFF));
        rect(x, y, x + 11, y + 10);
    }
    for (i = 0; i < 12; i++) {                          /* boutons B0..B11 */
        int x = 8 + (int)i * 26;
        put(DL_RGB(((s->buttons >> i) & 1) ? C_LIT : C_OFF));
        rect(x, 127, x + 23, 142);
    }
    put(DL_END);
    for (i = 0; i < 12; i++) {
        p = fmt_uint(buf, i);
        *p = 0;
        text(12 + (int)i * 26, 127, FONT_SMALL, C_DIM, buf);
    }
    for (i = 0; i < 15; i++) {                          /* potards : 3 rangées de 5 */
        p = fmt_str(buf, "P");
        p = fmt_uint(p, i + 1);
        p = fmt_str(p, " ");
        p = fmt_int(p, s->knob[i]);
        *p = 0;
        text(8 + (int)(i % 5) * 62, 145 + (int)(i / 5) * 15, FONT_SMALL, C_TEXT, buf);
    }
    p = fmt_str(buf, "enc ");
    p = fmt_int(p, s->enc);
    p = fmt_str(p, "   tours/s ");
    p = fmt_uint(p, s->loops_ps);
    *p = 0;
    text(8, 191, FONT_SMALL, C_TEXT, buf);
    p = fmt_str(buf, "latence ");
    p = fmt_uint(p, s->lat_last);
    p = fmt_str(p, " ms  moy ");
    p = fmt_uint(p, s->lat_n ? s->lat_sum / s->lat_n : 0);
    p = fmt_str(p, "  max ");
    p = fmt_uint(p, s->lat_max);
    *p = 0;
    text(8, 207, FONT_SMALL, C_DIM, buf);
    text(8, 223, FONT_SMALL, C_ACC, s->msg ? s->msg : "Shift + clic encodeur (2 s) : Play+ d'origine");
    if (k) {                                            /* dernier événement */
        static const char *const nm[5] = {"", "pad ", "bouton ", "potard ", "encodeur "};
        p = fmt_str(buf, nm[k < 5 ? k : 0]);
        if (k == 1) {
            p = fmt_str(p, "r");
            p = fmt_uint(p, a / 20);
            p = fmt_str(p, " c");
            p = fmt_uint(p, a % 20);
        } else if (k != 4)
            p = fmt_uint(p, a);
        if (k <= 2)
            p = fmt_str(p, st == 0 ? " appui" : " relache");
        else
            p = fmt_int(fmt_str(p, " "), s->last_d);
        *p = 0;
        text(200, 2, FONT_SMALL, C_TEXT, buf);
    }
    put(DL_DISPLAY);
    put(CMD_SWAP);
    fw_eve_end();
    s->frames++;
}

/* Remplace l'appel de l'automate d'affichage de Polyend (bl en 0x600aab34, dans fw_hw_update). */
void cfw_own_display(void *dm)
{
    struct own *s = own();
    if (!s || !s->active) {
        if (s && s->redraw && U32(dm, DM_READY) == 1 && U8(dm, DM_STATE) == 0) {
            s->redraw = 0;
            U8(dm, DM_STATE) = 2;       /* Polyend recompose son image depuis ses caches (intacts) */
        }
        fw_display_update(dm);
        return;
    }
    if (U32(dm, DM_READY) != 1 || !PTR(dm, 4))
        return;
    if (!s->frames && fw_millis - s->t0 > FIRST_FRAME_MS) {
        s->give_up = 1;                 /* notre image ne part pas : la boucle rendra la main */
        return;
    }
    if (!s->dirty || fw_millis - s->frame_ms < FRAME_MS || !fw_eve_idle())
        return;
    s->dirty = 0;
    s->frame_ms = fw_millis;
    if (s->ev_ms) {                                     /* latence : événement -> image envoyée */
        u32 lat = fw_millis + 1 - s->ev_ms;
        s->ev_ms = 0;
        s->lat_last = lat;
        if (lat > s->lat_max)
            s->lat_max = lat;
        s->lat_sum += lat;
        s->lat_n++;
    }
    if (og_pgi(s))
        og_draw(s);
    else
        draw(s);
}

/* ---- la boucle -------------------------------------------------------------------------------- */

static void zero(void *p, u32 n)
{
    volatile u8 *b = (volatile u8 *)p;
    while (n--)
        *b++ = 0;
}

/* Ce que fait la boucle principale de l'officiel (0x600ad852..0x600ad8be), une fois la main rendue. */
static void stock_loop(void)
{
    u32 t_blink = fw_millis + 500, t_task = fw_millis + 5000, led = 0;
    for (;;) {
        void *app;
        fw_bg_services();
        fw_hw_update(fw_hw);
        fw_bg_service2(fw_bg_service2_obj);
        app = fw_app_holder;
        VFN(app, 4, void)(app);
        if (fw_millis > t_blink) {
            fw_gpio_write((void *)0x401c0000u, 26, led);   /* battement, comme l'officiel */
            led ^= 1;
            t_blink = fw_millis + 500;
        }
        if (fw_millis > t_task) {
            fw_periodic_5s(fw_periodic_5s_obj);
            t_task = fw_millis + 5000;
        }
    }
}

static void give_back(struct own *s)
{
    u32 k;
    for (k = 0; k < NVOICE; k++)
        s->v[k].on = 0;
    fw_audio_render_cb = s->orig_render;
    *(const void *volatile *)fw_input_listener = s->orig_vt;
    s->active = 0;
    s->redraw = 1;
    stock_loop();
}

static void reboot_stock(void)
{
    volatile u32 *w = (volatile u32 *)cfw_ram;
    w[1] = STOCK_MAGIC;                 /* lu par cfw_own_main au démarrage suivant */
    w[2] = ~STOCK_MAGIC;
    __asm__ volatile("dsb" ::: "memory");
    *(volatile u32 *)0xE000ED0Cu = 0x05FA0004u;   /* AIRCR : SYSRESETREQ */
    for (;;)
        ;
}

/* Remplace l'appel de la fonction vide en 0x600ad854 (début de la boucle principale) : ne revient jamais. */
void cfw_own_main(void *unused)
{
    struct own *s;
    (void)unused;
    {
        volatile u32 *w = (volatile u32 *)cfw_ram;
        if (w[1] == STOCK_MAGIC && w[2] == ~STOCK_MAGIC)
            return;                     /* demandé avant le redémarrage : le Play+ d'origine, ce démarrage-ci */
    }
    if (fw_millis < TAKE_MS || U8(fw_hw + 4, DM_STATE) != 0 || !fw_eve_idle())
        return;                         /* pas encore : l'officiel fait ce tour normalement */
    s = (struct own *)fw_operator_new(sizeof(struct own));
    zero(s, sizeof(struct own));
    s->self = s;
    s->t0 = fw_millis;
    s->magic = OWN_MAGIC;
    *(struct own *volatile *)cfw_ram = s;
    s->orig_vt = *(const void *volatile *)fw_input_listener;
    s->orig_render = fw_audio_render_cb;
    *(const ev_fn *volatile *)fw_input_listener = own_listener_vt;   /* les contrôles viennent ici */
    fw_audio_render_cb = own_render;                                  /* une seule écriture 32 bits */
    s->active = 1;
    s->dirty = 1;
    s->led_dirty = 1;
    s->sec_ms = fw_millis;
    s->try_ms = fw_millis;
    og_load(s);                         /* l'invité de la carte (LOOM-8), s'il y est */
    for (;;) {
        fw_bg_services();               /* interroge les contrôleurs des pads (I2C) */
        fw_hw_update(fw_hw);            /* contrôles -> notre table ; écran -> cfw_own_display */
        if (og_pgi(s)) {
            og_tick(s);
            if (fw_millis - s->bands_ms >= 250) {       /* bandes latérales et charge : 4 fois par seconde */
                s->bands_ms = fw_millis;
                s->dirty = 1;
            }
        } else {
            leds(s);
            if (fw_millis - s->try_ms >= RETRY_MS) {
                s->try_ms = fw_millis;
                if (og_load(s))
                    s->dirty = 1;
            }
        }
        if ((s->buttons >> BTN_POWER) & 1) {
            if (!s->power_ms)
                s->power_ms = fw_millis + 1;
            else if (fw_millis + 1 - s->power_ms >= POWER_MS)
                fw_power_off(fw_hw);
        } else
            s->power_ms = 0;
        if (s->give_up)
            give_back(s);               /* ne revient pas */
        if ((s->buttons & BTN_EXIT) == BTN_EXIT) {
            if (!s->exit_ms)
                s->exit_ms = fw_millis + 1;
            else if (fw_millis + 1 - s->exit_ms >= EXIT_MS) {
                if (og_pgi(s))
                    reboot_stock();     /* ne revient pas */
                give_back(s);           /* ne revient pas */
            }
        } else
            s->exit_ms = 0;
        s->loops++;
        if (fw_millis - s->sec_ms >= 1000) {
            s->sec_ms += 1000;
            s->loops_ps = s->loops;
            s->loops = 0;
            s->dirty = 1;
        }
    }
}
