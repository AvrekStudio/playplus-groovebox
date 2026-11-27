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
 * MENU D'ALLUMAGE (0.3.0) : à la prise de main, la liste des firmwares de la carte (dossier /Guests, fichiers .pgi), puis
 * « Boucle de test » et « Play+ d'origine ». Encodeur + clic (B6), ou un pad de la 1re rangée. Le curseur
 * part du dernier choix (/Guests/boot.txt). « Play+ d'origine » rend la main à chaud (give_back) : rien
 * n'a encore pris la mémoire des samples.
 * INVITÉ (own_guest.c) : contrôles, son, image et tâche de fond vont à lui (le toucher des potards
 * jamais ; l'interrupteur jamais).
 * INTERRUPTEUR (bouton 12) tenu POWER_MS : fw_power_off, comme l'officiel (sans sauvegarde de Polyend).
 * SORTIE DE SECOURS : Shift (B10) + B6 tenus EXIT_MS. Sans invité : on rend la main au Play+ d'origine
 * jusqu'au prochain redémarrage (give_back). Avec invité (il occupe la mémoire des samples de Polyend) :
 * redémarrage (SYSRESETREQ), donc retour au menu d'allumage. (0.2.x écrivait un drapeau dans cfw_ram
 * pour démarrer l'officiel après le reset : sur la machine il ne survit pas au reset, l'officiel
 * s'affichait un instant puis LOOM revenait. Abandonné pour le menu.)
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
 * coprocesseur de l'écran est occupé, on revient tout de suite. Ensuite on prend la main pour de bon.
 * 0.4.0 : TAKE_MS 3000 -> 200. Au virtuel la boucle de l'officiel démarre vers 1,96 s (son logo est
 * encore à l'écran) et il dessine son écran principal vers 2,1 s : avec 3 s, l'écran de Polyend
 * apparaissait un instant avant notre menu. On prend donc la main dès le premier tour de boucle où
 * l'affichage est au repos : tout son démarrage (setup) est déjà fait à ce moment-là. Si notre première image ne part pas dans les
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
extern int fw_codec_hp(void *codec, u32 left, u32 right);
extern u8 fw_codec[];
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
    if (!s || s->off)
        return 0;                       /* veille : silence */
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

static void zero(void *p, u32 n);

/* ---- menu d'allumage ---------------------------------------------------------------------------- */

/* entrée i du menu : 0..n-1 invités, n boucle de test, n + 1 Play+ d'origine. Noté seulement ; la boucle
 * fait le chargement (accès carte) hors de la table de listener. */
static u32 menu_count(const struct own_menu *m)
{
    return m->page == 0 ? m->n + 3 : m->page == 1 ? m->nf + 1 : 2;
}

static void menu_choose(struct own *s, u32 i)
{
    struct own_menu *m = (struct own_menu *)s->menu;
    if (i >= menu_count(m))
        return;
    m->cur = i;
    s->menu_pick = i + 1;
    s->dirty = 1;
    s->led_dirty = 1;
}

static int same(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}

/* curseur sur le dernier choix (/Guests/boot.txt). « ! » (0.6.1, écrit par to_stock avant un redémarrage) : rendre
 * la main à l'officiel tout de suite, boot.txt repasse à « - ». */
static void menu_cursor(struct own *s, struct own_menu *m)
{
    char last[MENU_NAME + 1];
    u32 i;
    m->cur = 0;
    if (og_last(last, sizeof last)) {
        if (same(last, "!")) {
            og_last_set("-");
            m->cur = m->n + 1;
            s->give_up = 1;
        } else if (same(last, "+"))
            m->cur = m->n;
        else if (same(last, "-"))
            m->cur = m->n + 1;
        else
            for (i = 0; i < m->n; i++)
                if (same(last, m->names[i]))
                    m->cur = i;
    }
}

/* crée le menu : invités de la carte, curseur sur le dernier choix */
static void menu_open(struct own *s)
{
    struct own_menu *m = (struct own_menu *)s->menu_mem;
    if (!m)                             /* (0.6.1) une seule fois : on y revient à chaque changement d'invité */
        m = (struct own_menu *)fw_operator_new(sizeof(struct own_menu));
    if (!m)
        return;
    s->menu_mem = (u32)m;
    zero(m, sizeof(struct own_menu));
    og_scan(m);
    menu_cursor(s, m);
    s->menu = (u32)m;
    s->dirty = 1;
    s->led_dirty = 1;
}

static void menu_leds(struct own *s)
{
    struct own_menu *m = (struct own_menu *)s->menu;
    u32 i;
    if (!s->led_dirty)
        return;
    s->led_dirty = 0;
    for (i = 0; i < 160; i++) {
        u32 r = 0, g = 0, b = 0;
        if (i < menu_count(m)) {
            if (m->page == 2)
                i == 0 ? (r = 30, g = 30, b = 30) : (r = 200, g = 0, b = 0);   /* non : gris ; oui : rouge */
            else if (m->page == 1)
                i < m->nf ? (r = 160, g = 0, b = 80) : (r = 30, g = 30, b = 30);  /* .ppf : magenta ; retour */
            else if (i < m->n)
                r = 0, g = 40, b = 160;         /* firmware : bleu */
            else if (i == m->n)
                r = 30, g = 30, b = 30;         /* boucle de test : gris */
            else if (i == m->n + 1)
                r = 160, g = 50, b = 0;         /* Play+ d'origine : orange */
            else
                r = 160, g = 0, b = 80;         /* mise à jour : magenta */
            if (i == m->cur)
                r = g = b = 255;                /* curseur : blanc */
        }
        fw_led_encode(fw_pad_leds, i, r, g, b);
    }
}

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
    if (s->menu) {
        if (b == 0 && a < 20)
            menu_choose(s, a);          /* 1re rangée : un pad par entrée */
        return;
    }
    if (og_pgi(s)) {
        if (og_pgi(s)->abi >= 5)                        /* (0.6.2) le 3e argument du pilote, pour l'étudier */
            og_event(s, PGI_EV_PADX, a, c);
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
    if (s->menu) {
        if (a == BTN_CLICK && b == 0 && !((s->buttons >> BTN_SHIFT) & 1))
            menu_choose(s, ((struct own_menu *)s->menu)->cur);
    } else if (og_pgi(s)) {
        if (a != BTN_POWER)
            og_event(s, PGI_EV_BUTTON, a, b);
        s->last_btn = (a + 1) | (b << 8);   /* affiché dans la bande de droite (diagnostic) */
        s->dirty = 1;
    }
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
    struct own *s = own();
    (void)l;
    (void)c;
    /* toucher des potards : ignoré, sauf pour un invité ABI 5 (0.6.2 : montrer un paramètre dès qu'on l'effleure) */
    if (s && !s->menu && og_pgi(s) && og_pgi(s)->abi >= 5 && a < 15)
        og_event(s, PGI_EV_TOUCH, a, b);
}

static void ev_encoder(void *l, u32 a, u32 b, u32 c)
{
    struct own *s = own();
    (void)l;
    (void)a;
    (void)c;
    if (s->menu) {
        struct own_menu *m = (struct own_menu *)s->menu;
        s32 c = (s32)m->cur + (s32)b, last = (s32)menu_count(m) - 1;
        m->cur = (u32)(c < 0 ? 0 : c > last ? last : c);
        s->dirty = 1;
        s->led_dirty = 1;
    } else if (og_pgi(s))
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

/* menu d'allumage : titre, entrées (curseur sur fond orange), aide */
#define MENU_ROWS 10u                   /* lignes visibles du menu (y 34 à 196 ; 202 : « aucun », 220 : message) */
static void draw_menu(struct own *s)
{
    struct own_menu *m = (struct own_menu *)s->menu;
    u32 i, k, total = menu_count(m), top = 0, rows;
    char buf[MENU_NAME + 4];
    if (m->cur >= MENU_ROWS)                    /* (0.6.0) plus de 10 lignes : la fenêtre suit le curseur */
        top = m->cur - (MENU_ROWS - 1u);
    rows = total - top < MENU_ROWS ? total - top : MENU_ROWS;
    fw_eve_begin();
    put(CMD_DLSTART);
    put(DL_CLEAR_RGB(C_BG));
    put(DL_CLEAR_ALL);
    put(DL_VFORMAT_1PX);
    text(8, 4, FONT_LARGE, C_ACC, "AVREK OS");
    text(150, 10, FONT_SMALL, C_DIM, m->page == 1 ? "Mise a jour" : m->page == 2 ? "Confirmer" : CFW_VERSION_STRING);
    put(DL_BEGIN_RECTS);
    put(DL_RGB(C_LIT));
    rect(4, 37 + (int)(m->cur - top) * 18, 315, 54 + (int)(m->cur - top) * 18);   /* le texte est dessiné ~3 px sous son y */
    put(DL_END);
    for (i = top; i < top + rows; i++) {
        const char *nm;
        u32 color = i == m->cur ? 0 : (m->page == 0 ? i < m->n : m->page == 1 ? i < m->nf : i == 1) ? C_TEXT : C_DIM;   /* noir sur le curseur (C_BG en movw/movt gêne le vérificateur) */
        if (m->page == 1)
            nm = i < m->nf ? m->fw[i] : "Retour";
        else if (m->page == 2)
            nm = i == 0 ? "Non, retour" : "Oui, installer ce firmware";
        else if (i < m->n) {                 /* nom sans « .pgi » */
            for (k = 0; m->names[i][k] && k < MENU_NAME; k++)
                buf[k] = m->names[i][k];
            buf[k > 4 ? k - 4 : k] = 0;
            nm = buf;
        } else
            nm = i == m->n ? "Boucle de test" : i == m->n + 1 ? (s->trashed ? "Play+ d'origine (redemarre)" : "Play+ d'origine")
                                                              : "Mise a jour du firmware";
        text(12, 34 + (int)(i - top) * 18, FONT_SMALL, color, nm);
    }
    if (top)
        text(300, 34, FONT_SMALL, C_DIM, "^");
    if (top + rows < total)
        text(300, 34 + (int)(MENU_ROWS - 1u) * 18, FONT_SMALL, C_DIM, "v");
    if (m->page == 2) {
        text(8, 76, FONT_SMALL, C_LIT, m->fw[m->fsel]);
        text(8, 94, FONT_SMALL, C_DIM, "Installe au redemarrage (chargeur Polyend).");
        text(8, 112, FONT_SMALL, C_DIM, "Ne pas eteindre pendant l'installation.");
    } else if (m->page == 1 && !m->nf)
        text(8, 202, FONT_SMALL, C_DIM, "Aucun PlayPlus*.ppf dans /Firmware");
    else if (m->page == 0 && !m->n)
        text(8, 202, FONT_SMALL, C_DIM, "Aucun firmware dans /Guests (*.pgi)");
    text(8, 220, FONT_SMALL, s->msg ? C_LIT : C_DIM,
         s->msg ? s->msg : "Encodeur + clic, ou pad de la 1re rangee");
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
    if (U32(dm, DM_READY) != 1 || !PTR(dm, 4) || s->off)
        return;
    if (!s->frames && fw_millis - s->t0 > FIRST_FRAME_MS) {
        s->give_up = 1;                 /* notre image ne part pas : la boucle rendra la main */
        return;
    }
    /* (0.5.4) pas de fw_eve_idle pendant un envoi eDMA : c'est une lecture SPI de l'EVE (CS bas/haut, TCR
     * réécrit par le pilote) au milieu de la transaction de l'image : elle la coupait et débloquait le
     * RXMSK, d'où le LPSPI3 bloqué FIFO de réception pleine vu dans dma.txt de 0.5.1 et 0.5.3 (le 3e
     * transfert tombait sur la première image des bandes : CITER 1bdb sur 1e03, FSR 00100001). */
    if (!s->dirty || fw_millis - s->frame_ms < FRAME_MS || s->dma_busy || !fw_eve_idle())
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
    else if (s->menu)
        draw_menu(s);
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

/* Redémarrage complet (SYSRESETREQ) : on repasse par le menu d'allumage. */
static void reboot(void)
{
    __asm__ volatile("dsb" ::: "memory");
    *(volatile u32 *)0xE000ED0Cu = 0x05FA0004u;   /* AIRCR : SYSRESETREQ */
    for (;;)
        ;
}

/* (0.6.1) Play+ d'origine. À chaud si rien n'a touché aux samples du projet ; sinon un invité les a écrasés :
 * « ! » dans /Guests/boot.txt puis redémarrage, et au démarrage suivant le menu rend la main tout de suite
 * (le projet est rechargé par l'officiel). Le drapeau en RAM ne survit pas au reset sur la machine (0.3.0). */
static void to_stock(struct own *s)
{
    og_last_set(s->trashed ? "!" : "-");
    if (s->trashed)
        reboot();                       /* ne revient pas */
    s->give_up = 1;
}

/* Shift + clic tenus avec un invité ou la boucle de test : retour au menu, sans redémarrer */
static void to_menu(struct own *s)
{
    u32 k;
    if (og_pgi(s))
        og_unload(s);
    for (k = 0; k < NVOICE; k++)        /* (boucle de test) */
        s->v[k].on = 0;
    s->msg = 0;
    menu_open(s);
    s->exit_lock = 1;                   /* Shift + clic encore tenus : rien tant qu'on ne les lâche pas */
    s->dirty = 1;
    s->led_dirty = 1;
}

/* Remplace l'appel de la fonction vide en 0x600ad854 (début de la boucle principale) : ne revient jamais. */
void cfw_own_main(void *unused)
{
    struct own *s;
    (void)unused;
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
    menu_open(s);                       /* menu d'allumage */
    for (;;) {
        fw_bg_services();               /* interroge les contrôleurs des pads (I2C) */
        fw_hw_update(fw_hw);            /* contrôles -> notre table ; écran -> cfw_own_display */
        if (s->off) {                   /* veille (extinction ratée) : seul l'interrupteur compte */
            if (s->off_ms && fw_millis + 1 - s->off_ms > OFF_ARM_MS && ((s->buttons >> BTN_POWER) & 1))
                reboot();               /* ne revient pas */
            continue;
        }
        if (og_pgi(s)) {
            og_tick(s);
            og_flush(s);
            if (fw_millis - s->bands_ms >= 250) {       /* bandes latérales et charge : 4 fois par seconde */
                s->bands_ms = fw_millis;
                s->dirty = 1;
            }
        } else if (s->menu) {
            struct own_menu *m = (struct own_menu *)s->menu;
            menu_leds(s);
            if (!m->n && fw_millis - s->scan_ms >= 2000) {  /* carte sans firmware : on regarde à nouveau */
                s->scan_ms = fw_millis;
                if (og_scan(m)) {
                    menu_cursor(s, m);
                    s->dirty = 1;
                    s->led_dirty = 1;
                }
            }
            if (s->menu_pick && m->page) {     /* (0.6.3) pages de la mise à jour */
                u32 i = s->menu_pick - 1;
                s->menu_pick = 0;
                s->msg = 0;
                if (m->page == 1 && i < m->nf) {
                    m->fsel = i;
                    m->page = 2;
                    m->cur = 0;         /* « Non » d'abord */
                } else if (m->page == 2 && i == 1) {
                    if (og_fw_request(m->fw[m->fsel])) {
                        og_dma_wait(s);
                        reboot();       /* le chargeur installe ; ne revient pas */
                    }
                    s->msg = "ecriture de /Firmware/_fwinfo impossible";
                } else if (m->page == 2) {
                    m->page = 1;
                    m->cur = m->fsel;
                } else {                /* « Retour » */
                    m->page = 0;
                    m->cur = m->n + 2;
                }
                s->dirty = 1;
                s->led_dirty = 1;
            } else if (s->menu_pick) {
                u32 i = s->menu_pick - 1;
                s->menu_pick = 0;
                if (i < m->n) {         /* un firmware : /Guests/<nom> */
                    char path[8 + MENU_NAME];
                    u32 k = 0, j;
                    for (j = 0; "/Guests/"[j]; j++)
                        path[k++] = "/Guests/"[j];
                    for (j = 0; m->names[i][j]; j++)
                        path[k++] = m->names[i][j];
                    path[k] = 0;
                    s->msg = 0;
                    if (og_load(s, path)) {
                        og_last_set(m->names[i]);
                        fw_codec_hp(fw_codec, HP_ATT, HP_ATT);
                        s->menu = 0;
                    } else if (!s->msg)
                        s->msg = "chargement impossible";
                } else if (i == m->n) { /* boucle de test */
                    og_last_set("+");
                    s->menu = 0;
                    s->msg = 0;
                } else if (i == m->n + 1)   /* Play+ d'origine : à chaud, ou par redémarrage (to_stock) */
                    to_stock(s);
                else {                  /* (0.6.3) mise à jour : les .ppf de /Firmware */
                    og_fw_scan(m);
                    m->page = 1;
                    m->cur = 0;
                    s->msg = 0;
                }
                s->dirty = 1;
                s->led_dirty = 1;
            }
        } else
            leds(s);
        if ((s->buttons >> BTN_POWER) & 1) {
            if (!s->power_ms)
                s->power_ms = fw_millis + 1;
            else if (fw_millis + 1 - s->power_ms >= POWER_MS) {
                /* fw_power_off éteint écran et LED, attend qu'on lâche l'interrupteur puis relâche la
                 * broche qui maintient l'alimentation : normalement on ne revient pas. Si on revient,
                 * l'alimentation n'a pas été coupée (cas vu sur la machine le 26/11 : « ne se rallume
                 * pas » : processeur toujours en marche, écran noir, chaque appui relançait
                 * l'extinction). On passe alors en veille : silence, rien à l'écran, et le prochain
                 * appui sur l'interrupteur redémarre Avrek OS. */
                s->off = 1;
                og_dma_wait(s);         /* (l'image en cours d'envoi : finie avant d'éteindre l'écran) */
                fw_power_off(fw_hw);
                s->buttons &= ~(1u << BTN_POWER);
                s->power_ms = 0;
                s->off_ms = fw_millis + 1;
            }
        } else
            s->power_ms = 0;
        if (s->give_up)
            give_back(s);               /* ne revient pas */
        if ((s->buttons & BTN_EXIT) == BTN_EXIT && !s->exit_lock) {
            if (!s->exit_ms)
                s->exit_ms = fw_millis + 1;
            else if (fw_millis + 1 - s->exit_ms >= EXIT_MS) {
                s->exit_ms = 0;
                if (s->menu && ((struct own_menu *)s->menu)->page) {   /* (0.6.3) pages de mise à jour : retour au menu */
                    struct own_menu *m = (struct own_menu *)s->menu;
                    m->page = 0;
                    m->cur = m->n + 2;
                    s->exit_lock = 1;
                    s->dirty = 1;
                    s->led_dirty = 1;
                } else if (s->menu)
                    to_stock(s);        /* depuis le menu : Play+ d'origine */
                else
                    to_menu(s);         /* (0.6.1) invité ou boucle de test : menu, sans redémarrer */
            }
        } else {
            s->exit_ms = 0;
            if (!(s->buttons & BTN_EXIT))
                s->exit_lock = 0;
        }
        s->loops++;
        if (fw_millis - s->sec_ms >= 1000) {
            s->sec_ms += 1000;
            s->loops_ps = s->loops;
            s->loops = 0;
            s->dirty = 1;
        }
    }
}
