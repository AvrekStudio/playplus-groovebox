/* Vector37 (genkigenki/Vector37, GPL-3.0, M. Geisler) en invité PGI sur le Play+ (Avrek OS).
 *
 * Principe : comme pour LOOM, on ne touche à rien dans Vector37. Son simulateur et ses tests compilent le
 * firmware sur PC contre un « panneau » simulé, tests/panel.h (moteur, interface, écran 240 x 240 : les mêmes
 * sources que sur le clavier). On inclut ce panel.h tel quel et on le relie au Play+ :
 *   - SON (sous interruption) : synth_block, par blocs de CTL trames, 44,1 kHz, en int16 ;
 *   - BOUCLE (tick, boucle principale) : toutes les UI_LOOP_MS, ce que fait panel_step (entrées, LED,
 *     écran) ; l'horloge du firmware (fm1_ms) suit millis de l'hôte. panel_render, qui fait les deux à la fois
 *     dans le simulateur (la boucle pendant le calcul du son), n'est pas utilisé : chez nous le son tourne
 *     sous interruption, l'interface jamais ;
 *   - ÉCRAN : fb (RGB565 natif, panel.h) envoyé à l'hôte quand une image a été dessinée (l'hôte ne renvoie
 *     que les rangées changées) ;
 *   - ENTRÉES : la matrice docs/correspondance/vector37.json, compilée en tables (gen_map_v37.py) ;
 *   - MÉMOIRE (sons USER, réglages) : la zone de données du NOR simulé (0x9D000 .. 0xBB000), lue au démarrage
 *     et réécrite après un SAVE dans /Guests/VECTOR37/store.bin.
 * Licences : Vector37 GPL-3.0 ; le .pgi est pour un usage personnel, ne pas le distribuer. */
#include <stdint.h>
#ifndef UI_LOOP_MS
#define UI_LOOP_MS 16u                          /* (draw.c : la boucle au plus toutes les 16 ms) */
#endif
#include "panel.h"                              /* Vector37/tests/panel.h, intact (-I du clone) */
#include "pgi_guest.h"
#include "map_vector37.h"                       /* généré depuis la matrice */

#define STORE_OFF 0x9D000u                      /* la zone de données du NOR (panel.h, st_erase / st_prog) */
#define STORE_LEN (0xBB000u - 0x9D000u)
#define STORE_FILE "store.bin"
#define STORE_QUIET_MS 1500u                    /* écrit sur la carte 1,5 s après le dernier SAVE */
#define VEL_KEY 100u
#define VEL_PAD 110u
#define PITCH_REST 540
#define PITCH_LO 450
#define PITCH_HI 630
#define PITCH_STEP 6                            /* un cran de potard : 6 sur 90 d'un côté */
#define PITCH_BACK_MS 250u                      /* la roue de pitch revient au centre sans cran depuis 250 ms */
#define CTL_RESYNC_MS 1000u                     /* fader / roue mod : repart de la valeur du son après 1 s */

static const struct pgi_host *host;
static uint32_t last_step, last_draws = 0xffffffffu, store_erases, store_ms;
static volatile uint32_t pp_shift;              /* Shift du Play+ (B10) : la couche */
static int32_t fad[4], mod_s;                   /* faders (0..127) et roue mod (0..130), au potard */
static uint32_t fad_ms[4], mod_ms, pitch_ms;
static int32_t pitch = PITCH_REST;
static uint64_t keys_down;                      /* touches tenues (pour leurs LED) */
static uint32_t led_cache[160];

/* ---- valeur actuelle d'un fader ou de la roue mod dans le son (comme le simulateur, sim_ctl) ---- */
static int32_t ctl_now(uint32_t i)
{
    if (i < 4u) {
        int32_t v = pt.g[FADER_G[i]], bd = 1 << 30, lo = 0, hi = 0, r;
        for (r = 0; r < 128; r++) {             /* la place du fader qui donne la valeur du son */
            int32_t d = fader_value(i, (uint32_t)r) - v;
            d = d < 0 ? -d : d;
            if (d < bd)
                bd = d, lo = hi = r;
            else if (d == bd && r == hi + 1)
                hi = r;
        }
        return (lo + hi) / 2;
    }
    return 12 + (pt.g[G_SUBSPD] * 118 + 63) / 127;   /* la roue mod : 0..130 (raw = 615 - s) */
}

/* ---- une entrée de la matrice ---- */
static void turn_target(const struct v37_map *m, int32_t d);
static void tap_target(const struct v37_map *m, uint32_t down)
{
    if (m->t == VT_ENC || m->t == VT_FADER || m->t == VT_WHEEL) {   /* un appui = un cran (action de la matrice) */
        if (down)
            turn_target(m, m->act == 2u ? -1 : 1);
        return;
    }
    switch (m->t) {
    case VT_BTN:
        panel_button(m->id, down);
        break;
    case VT_KEY:
        panel_key(m->id, down, VEL_KEY);
        if (down)
            keys_down |= (uint64_t)1u << m->id;
        else
            keys_down &= ~((uint64_t)1u << m->id);
        break;
    case VT_PAD:
        panel_pad(m->id, down ? VEL_PAD : 0u);
        break;
    default:
        break;
    }
}

static void turn_target(const struct v37_map *m, int32_t d)
{
    uint32_t now = host->millis();
    if (m->t == VT_ENC) {
        panel_enc(m->id, d);
    } else if (m->t == VT_FADER && m->id < 4u) {
        if (now - fad_ms[m->id] > CTL_RESYNC_MS)
            fad[m->id] = ctl_now(m->id);
        fad_ms[m->id] = now;
        fad[m->id] += d;
        fad[m->id] = fad[m->id] < 0 ? 0 : fad[m->id] > 127 ? 127 : fad[m->id];
        panel_fader(m->id, (uint32_t)fad[m->id]);
    } else if (m->t == VT_WHEEL && m->id == W_MOD) {
        if (now - mod_ms > CTL_RESYNC_MS)
            mod_s = ctl_now(4u);
        mod_ms = now;
        mod_s += d;
        mod_s = mod_s < 0 ? 0 : mod_s > 130 ? 130 : mod_s;
        panel_wheel(1u, (uint32_t)(615 - mod_s));
    } else if (m->t == VT_WHEEL && m->id == W_PITCH) {
        pitch += d * PITCH_STEP;
        pitch = pitch < PITCH_LO ? PITCH_LO : pitch > PITCH_HI ? PITCH_HI : pitch;
        pitch_ms = now;
        panel_wheel(0u, (uint32_t)pitch);
    } else if (m->t != VT_NONE) {               /* un bouton, une touche, un pad : un appui par cran */
        int32_t n = d < 0 ? -d : d;
        while (n-- > 0) {
            tap_target(m, 1u);
            tap_target(m, 0u);
        }
    }
}

/* couche Shift : l'entrée de la couche 1 si elle existe et Shift est tenu */
#define PICK(TAB, i) (pp_shift && TAB[1][i].t != VT_NONE ? &TAB[1][i] : &TAB[0][i])

static void event(unsigned int kind, unsigned int a, unsigned int b)
{
    if (kind == PGI_EV_BUTTON && a == 10u) {    /* Shift du Play+ : la couche, rien d'autre */
        if (b != 2u)
            pp_shift = (b == 0u);
        return;
    }
    if (kind == PGI_EV_BUTTON && a < 12u && b != 2u)
        tap_target(PICK(MAP_BTN, a), b == 0u);
    else if (kind == PGI_EV_PAD && a < 160u && b != 2u)
        tap_target(PICK(MAP_PAD, a), b == 0u);
    else if (kind == PGI_EV_KNOB && a < 15u) {
        int32_t d = (int32_t)b;                 /* cran + : entrée [0] ; cran − : [1] si elle existe (boutons) */
        const struct v37_map *m = PICK(MAP_KNOB, a * 2u);
        const struct v37_map *mm = PICK(MAP_KNOB, a * 2u + 1u);
        if (d < 0 && mm->t != VT_NONE)
            turn_target(mm, -d);
        else
            turn_target(m, d);
    } else if (kind == PGI_EV_ENCODER) {
        int32_t d = (int32_t)b;
        const struct v37_map *m = PICK(MAP_ENC, 0u), *mm = PICK(MAP_ENC, 1u);
        if (d < 0 && mm->t != VT_NONE)
            turn_target(mm, -d);
        else
            turn_target(m, d);
    }
}

/* ---- LED des pads du Play+ : pads du SMK à leur couleur, boutons allumés / faibles, touches blanches /
 * noires faibles et vives tenues ---- */
static void leds(void)
{
    uint32_t i;
    for (i = 0; i < 160u; i++) {
        const struct v37_map *m = &MAP_PAD[0][i];
        uint32_t rgb = 0;
        if (m->t == VT_PAD && m->id < 16u)
            rgb = pad_rgb[m->id];
        else if (m->t == VT_BTN)
            rgb = (btn_leds >> m->id & 1u) ? 0x40c0ffu : 0x041828u;
        else if (m->t == VT_KEY)
            rgb = (keys_down >> m->id & 1u) ? 0xffffffu : (V37_BLACK >> (m->id % 12u) & 1u) ? 0x301000u : 0x202020u;
        if (rgb != led_cache[i]) {
            led_cache[i] = rgb;
            host->led(i, rgb >> 16 & 0xffu, rgb >> 8 & 0xffu, rgb & 0xffu);
        }
    }
}

/* ---- son : sous interruption ---- */
static void audio(short *l, short *r, unsigned int n)
{
    unsigned int i;
    for (i = 0; i < n; i++) {
        int32_t a, b;
        if (pblk_pos >= CTL) {
            synth_block(pblk, CTL);
            pblk_pos = 0;
        }
        a = pblk[2u * pblk_pos];
        b = pblk[2u * pblk_pos + 1u];
        l[i] = (short)(a < -32768 ? -32768 : a > 32767 ? 32767 : a);
        r[i] = (short)(b < -32768 ? -32768 : b > 32767 ? 32767 : b);
        pblk_pos++;
    }
}

/* ---- boucle ---- */
static void tick(void)
{
    uint32_t now = host->millis();
    if (now - last_step < UI_LOOP_MS)
        return;
    last_step = now;
    fm1_ms = now;                               /* l'horloge du firmware (panel_render l'avançait avec le son) */
    if (pitch != PITCH_REST && now - pitch_ms > PITCH_BACK_MS) {
        pitch = PITCH_REST;                     /* la roue de pitch est à ressort */
        panel_wheel(0u, (uint32_t)pitch);
    }
    panel_step();                               /* entrées, MIDI (rien), LED, écran : panel.h */
    if (fb_draws != last_draws && host->abi >= 2 && host->screen) {
        last_draws = fb_draws;
        host->screen(fb, 0, 240, 0);            /* fb : RGB565 natif (panel.h a permuté les octets du SPI) */
    }
    leds();
    if (panel_erases != store_erases) {         /* un SAVE (une page effacée) : sur la carte, une fois au calme */
        if (!store_ms)
            store_ms = now + 1u;
        else if (now + 1u - store_ms >= STORE_QUIET_MS) {
            store_erases = panel_erases;
            store_ms = 0;
            if (host->abi >= 3 && host->file_write)
                host->file_write(STORE_FILE, panel_nor + STORE_OFF, STORE_LEN);
        }
    }
}

static void init(const struct pgi_host *h)
{
    uint32_t i;
    host = h;
    panel_init();                               /* NOR effacé, puis le démarrage du firmware */
    if (h->abi >= 3 && h->file_read && h->file_read(STORE_FILE, panel_nor + STORE_OFF, STORE_LEN) == (int)STORE_LEN)
        panel_init();                           /* (comme le simulateur : la mémoire rendue, puis un redémarrage) */
    store_erases = panel_erases;
    for (i = 0; i < 4u; i++)
        fad[i] = fm1_in.fader[i];
    mod_s = ctl_now(4u);
    for (i = 0; i < 160u; i++)
        led_cache[i] = 0xff000000u;
    last_step = h->millis();
    fm1_ms = last_step;
}

PGI_GUEST("VECTOR37", init, audio, event, tick);
