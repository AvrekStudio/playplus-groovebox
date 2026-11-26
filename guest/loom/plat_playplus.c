/* LOOM-8 (charlesvestal/fm1-loom, sur fm1-base) en invité PGI sur le Play+ : la plateforme d'fm1-base
 * (base/include/plat.h) réalisée au-dessus des services du mode invité (cfw/src/pgi.h).
 *
 * Code à nous (AvrekStudio). Il est compilé dans la même unité que l'app LOOM-8 (app.c, MPL-2.0 / le tout
 * GPL-3.0 : licences de LOOM-8 et d'fm1-base, voir leur dépôt) : usage personnel, rien n'est distribué.
 * Construction : guest/loom/build_loom.py (clone de LOOM-8 à un commit fixé, non versionné ici).
 *
 * Ce qui est fait :
 *   - écran : lcd_fill / lcd_blit (gfx.c, octets permutés comme le panneau du FM-1) dans pp_fb, 240 x 240 ;
 *     les rangées modifiées partent à l'écran du Play+ par pgi_host.screen à chaque tour de boucle ;
 *   - son : app_render par blocs de APP_BLOCK = 256 trames, 24 bits ; le Play+ en demande 128 en int16 :
 *     un bloc LOOM sur deux appels (le second rend la seconde moitié), conversion 24 -> 16 bits ;
 *   - commandes : la matrice validée (docs/correspondance/loom-8.json → map_playplus.h, gen_map.py) ;
 *   - boucle : le FM-1 lit les commandes toutes les ms (app_input_only) et dessine toutes les 16 ms
 *     (app_frame) : même cadence ici, au fil de pgi tick ;
 *   - LED : plat_lights -> LED des pads (clavier et boutons) ;
 *   - ressources (909 cymbales, grands caractères) : res.bin de LOOM-8 intégré à l'image (res_blob.S).
 * Ce qui ne l'est pas (bouchons) : second cœur (LOOM fait tout sur le premier), Bluetooth, USB, MIDI,
 * banque de samples utilisateur. Sauvegarde : fichiers sur la carte SD (/Guests/LOOM-8/, voir plus bas). */
#include <stdint.h>
#include <string.h>
#include "plat.h"
#include "app.h"
#include "fm1b_config.h"
#include "pgi_guest.h"
#ifndef APP_VERSION
#define APP_VERSION "PLAY+"
#endif

static const struct pgi_host *host;

/* ---- écran : 240 x 240 RGB565 octets permutés ------------------------------------------------- */
static uint16_t pp_fb[240 * 240];
static uint32_t pp_d0 = 240, pp_d1 = 0;      /* rangées modifiées depuis le dernier envoi [d0, d1) */
static void lcd_sync(void) {}
static void pp_touch(uint32_t y, uint32_t h)
{
    if (y < pp_d0)
        pp_d0 = y;
    if (y + h > pp_d1)
        pp_d1 = y + h > 240u ? 240u : y + h;
}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    uint16_t sc = (uint16_t)(((c >> 8) & 0xFFu) | ((c & 0xFFu) << 8));
    for (j = y; j < y + h && j < 240u; j++)
        for (i = x; i < x + w && i < 240u; i++)
            pp_fb[j * 240u + i] = sc;
    pp_touch(y, h);
}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    uint32_t i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (x + i < 240u && y + j < 240u)
                pp_fb[(y + j) * 240u + x + i] = px[j * w + i];
    pp_touch(y, h);
}

/* ---- ce que plat_loom.c (LOOM-8 sur le FM-1) attend de la plateforme du FM-1 ------------------- */
#define DWT_CYCCNT (*(volatile uint32_t *)0xE0001004u)    /* compteur de cycles du M7, activé par l'officiel */
#define FM1_TICKS_PER_US 600u
static inline uint32_t fm1_ticks(void) { return DWT_CYCCNT; }
#define FM1_CRASH_MAGIC 0x43524153u
static struct { uint32_t magic, vec, pc, emu, dbg, rets; } fm1_crash;
static const int flash_ok = 1;
static int st_load_prev(uint32_t obj, void *dst, uint32_t max) { (void)obj; (void)dst; (void)max; return -1; }
static int fm1_vdc14(void) { return 0; }
static int fm1_sysvdd(void) { return 0; }

#include "gfx.c"
/* lecteur d'écran (annonces vers un ordinateur) : pas de liaison sur le Play+ */
static int ann_wire_send(const uint8_t *p, uint32_t n) { (void)p; (void)n; return -1; }
#include "ann_words.h"
#include "announce.c"
#include "btdev.c"
#include "app.c"

/* ---- commandes : correspondance Play+ -> FM-1, d'après la matrice validée -----------------------
 * map_playplus.h est généré (gen_map.py) depuis docs/correspondance/loom-8.json, faite dans la page
 * « Câblage FM-1 → Play+ ». Deux couches : sans et avec la touche Shift du Play+ (bouton 10), qui sert
 * de couche à nous et n'est jamais transmise à LOOM. Avec Shift, une source sans entrée dans la couche
 * Shift garde son rôle normal. Le toucher des potards n'est jamais transmis (l'hôte le jette déjà).
 * Actions sur un encodeur depuis un pad ou un bouton : cran +, cran -, ou « valeur N » (seulement pour
 * ALGORITHM : la piste N, voir algo_value). Combinaisons (« HOME + SELECT »...) : le bouton du FM-1 est
 * tenu pendant l'action (MOD_HOLD_MS après le dernier cran d'un potard). */
#include "map_playplus.h"
#include "seq/pattern.h"                            /* AUX_BRAIN, AUX_PUNCH, AUX_WEAVE (liste des pistes) */
#define MOD_HOLD_MS 250u

static volatile uint32_t in_btn, in_keys;          /* tenus */
static volatile uint32_t rel_btn, rel_keys;        /* relâchés pas encore vus : gardés un tour */
static volatile int32_t in_enc[NE];
static volatile uint32_t in_master = 2896;         /* ~ -6 dB (APP_GAIN : carré) */
static volatile uint32_t pp_shift;                 /* Shift du Play+ tenu */
static volatile uint32_t mod_bit, mod_until;       /* bouton FM-1 tenu par une combinaison tournée, jusqu'à */

uint32_t plat_buttons(void)
{
    uint32_t b = in_btn;
    in_btn &= ~rel_btn;                             /* un appui bref compte une fois au moins */
    rel_btn = 0;
    if (mod_bit && (int32_t)(mod_until - host->millis()) > 0)
        b |= mod_bit;
    else
        mod_bit = 0;
    return b;
}
uint32_t plat_keys(void)
{
    uint32_t k = in_keys;
    in_keys &= ~rel_keys;
    rel_keys = 0;
    return k;
}
int32_t plat_enc(int role)
{
    int32_t v;
    if (role < 0 || role >= NE)
        return 0;
    v = in_enc[role];
    in_enc[role] = 0;
    return v;
}
uint32_t plat_master(void) { return in_master; }

static void press(volatile uint32_t *held, volatile uint32_t *rel, int bit, uint32_t state)
{
    if (bit < 0)
        return;
    if (state == 0) {
        *held |= 1u << bit;
        *rel &= ~(1u << bit);
    } else if (state == 1) {
        *rel |= 1u << bit;                          /* effacé après la prochaine lecture */
    }
}

/* position de la piste t dans la liste que parcourt ALGORITHM (ui.c, track_step : T1..T8, COND, THROW,
 * WEAVE) ; les pistes hors liste (FX I, FX II du Mix) comptent comme la fin */
static int algo_index(int t)
{
    if (t >= 0 && t < 8)
        return t;
    return t == AUX_BRAIN ? 8 : t == AUX_PUNCH ? 9 : 10;
}
/* « valeur N » d'ALGORITHM : l'écart entre la piste courante (proj.trk_sel) et la piste N, en une fois.
 * track_step reçoit les crans bruts (pas d'accélération sur ALGORITHM, ui.c) : l'écart est exact. */
static void algo_value(uint32_t n)
{
    in_enc[EN_ALGO] += (int32_t)(n - 1u) - algo_index(proj.trk_sel);
}

/* ---- grille « éclatée » (26/11) : séquenceur en haut, notes en bas -------------------------------------
 * Les 64 pads des rangées 0 à 3 de la grille 16×8 (s'ils ne sont pas dans la matrice) = les 64 pas du
 * pattern (4 mesures de 16, NSTEP) de la piste choisie : rangée r = mesure r + 1. Un appui affiche la mesure
 * r dans LOOM (ui.bar) et lui donne la blanche c par key_down / key_up : tout son comportement reste (tapé =
 * trig posé ou retiré, tenu = pas édité aux potards, pas tenu + autre pas de la rangée = longueur, piste
 * batterie = le son choisi). Hors des pages Steps / Bar, un appui ouvre d'abord Steps (page_button(B_SEQ)).
 * Le clavier de la matrice joue TOUJOURS des notes sur Steps / Bar : keyboard_down (ce que LOOM fait quand
 * SEQ est tenu ou en enregistrement : jeu, REC + PLAY, pas à pas ; batterie : le son joué devient celui des
 * trigs). Ailleurs (Arrange, SAVE tenu, listes...) il reste le clavier normal de LOOM.
 * Rien n'est modifié dans LOOM : app.c est inclus plus haut, ses fonctions et variables static sont
 * visibles ici. Les appuis sont mis en file par event() et traités au début de tick(), hors de LOOM. */
#define SG_N 64u
#define SG_Q 32u
static uint32_t sg_q[SG_Q], sg_qw, sg_qr;           /* (genre << 16) | (n << 8) | état ; genre 1 pas, 2 touche */
static uint8_t sg_key[SG_N];                        /* touche donnée à LOOM pour ce pas tenu, + 1 (0 : aucune) */
static uint32_t kb_direct;                          /* touches jouées directement (keyboard_down) : leur relâché aussi */
static uint32_t sg_led[SG_N + 9u];                  /* couleur envoyée à chaque pad de pas, puis aux 9 pads SCALE
                                                     * (0xff000000 : jamais) */

static int sg_pad(uint32_t a)                       /* n° de pas du pad a, ou -1 */
{
    if (a >= 80u || a % 20u >= 16u || MAP_PAD[0][a].t != MT_NONE)
        return -1;
    return (int)((a / 20u) * 16u + a % 20u);
}
/* Rangée 6, colonnes 0 à 8 : SCALE de la page Bar (durée d'un pas en doubles-croches : 1, 2, 3, 4, 5, 6, 8, 16,
 * 1/2 ; BAR_SCALE_KEY de LOOM), que les noires 1 à 9 règlent sur Bar dans LOOM : le clavier y jouant des
 * notes, ces 9 pads donnent ces noires à LOOM, et seulement sur Bar, piste synthé, comme LOOM. */
static int sc_pad(uint32_t a)                       /* n° de 0 à 8 du pad SCALE a, ou -1 */
{
    if (a < 120u || a >= 129u || MAP_PAD[0][a].t != MT_NONE)
        return -1;
    return (int)(a - 120u);
}
static int sc_active(void)                          /* les conditions de LOOM pour ses noires sur Bar (ui.c) */
{
    int trk = sel_trk();
    return ui.page == P_BAR && is_seqt(trk) && !is_drum(trk) && !(ui.btn >> B_SEQ & 1u) && !ui.rec_armed &&
           !seq.rec && !seq.rec_wait && !ui.ask && !lst.open && !(ui.btn >> B_SAVE & 1u) && !home_held();
}

static void sg_push(uint32_t kind, uint32_t n, uint32_t state)
{
    if (sg_qw - sg_qr < SG_Q)
        sg_q[sg_qw++ % SG_Q] = kind << 16 | n << 8 | state;
}
static int sg_free_keys(void)                       /* le clavier peut jouer directement (Steps / Bar, rien d'autre) */
{
    return steps_page(ui.page) && !ui.ask && !lst.open && !(ui.btn >> B_SAVE & 1u) && !home_held();
}
static void sg_run(void)
{
    while (sg_qr != sg_qw) {
        uint32_t q = sg_q[sg_qr++ % SG_Q], kind = q >> 16, n = q >> 8 & 0xffu, st = q & 0xffu;
        if (kind == 3u) {                           /* SCALE (rangée 6) : appui seul */
            uint32_t k = BLACK_K[n];
            if (st == 0u && sc_active() && !(kb_direct >> k & 1u) && !ui.held) {
                ui.kmode[k] = KM_NONE;
                ui.key_t[k] = plat_ms();
                key_down((int)k);
                key_up((int)k);
                mark_dirty();
            }
            continue;
        }
        if (kind == 2u) {                           /* touche du clavier */
            if (st == 0u && !(kb_direct >> n & 1u) && sg_free_keys()) {
                kb_direct |= 1u << n;
                ui.kmode[n] = KM_NONE;
                ui.key_t[n] = plat_ms();
                keyboard_down((int)n);
                mark_dirty();
            } else if (st == 1u && (kb_direct >> n & 1u)) {
                kb_direct &= ~(1u << n);
                key_up((int)n);
                mark_dirty();
            } else
                press(&in_keys, &rel_keys, (int)n, st);   /* le clavier normal de LOOM */
        } else if (st == 0u) {                      /* pas : appui */
            uint32_t r = n / 16u, k = WHITE_K[n % 16u];
            sg_key[n] = 0;
            if (ui.ask || lst.open || (ui.btn >> B_SAVE & 1u) || home_held() || (kb_direct >> k & 1u))
                continue;
            if (!steps_page(ui.page)) {
                page_button(B_SEQ);
                ui.held = 0;
            }
            if (ui.held && ui.bar != r)             /* LOOM tient les pas mesure par mesure */
                continue;
            ui.bar = (uint8_t)r;
            ui.kmode[k] = KM_NONE;
            ui.key_t[k] = plat_ms();
            key_down((int)k);
            sg_key[n] = (uint8_t)(k + 1u);
            mark_dirty();
        } else if (sg_key[n]) {                     /* pas : relâché */
            key_up(sg_key[n] - 1);
            sg_key[n] = 0;
            mark_dirty();
        }
    }
}

/* LED des 64 pas : trig = orange (batterie : le son choisi orange, les autres sons orange faible), tête de
 * lecture = blanc, pas vide de la mesure affichée = bleu faible, des autres mesures = bleu très faible, au-delà
 * de la longueur du pattern = éteint. Seuls les pads qui changent sont réécrits. */
static void sg_leds(void)
{
    int trk = sel_trk(), ok = is_seqt(trk) && !lst.open, len = ok ? tpat(trk)->len : 0;
    int ph = ok && playing() ? eng_step[trk] % NSTEP : -1;
    uint32_t s;
    for (s = 0; s < SG_N; s++) {
        uint32_t pad = (s / 16u) * 20u + s % 16u, rgb = 0;
        if (sg_pad(pad) < 0)
            continue;
        if (ok && (int)s < len) {
            const step_t *st = &tpat(trk)->st[s];
            if ((int)s == ph)
                rgb = 0xffffff;
            else if (!step_empty(st))
                rgb = is_drum(trk) && !kit_has(st, ui.dsound + 1) ? 0x301400 : 0xff6000;
            else
                rgb = s / 16u == ui.bar && steps_page(ui.page) ? 0x000030 : 0x00000a;
        }
        if (rgb != sg_led[s]) {
            sg_led[s] = rgb;
            host->led(pad, rgb >> 16, rgb >> 8 & 0xffu, rgb & 0xffu);
        }
    }
    for (s = 0; s < 9u; s++) {                      /* SCALE : allumés sur Bar seulement, la valeur en blanc */
        uint32_t pad = 120u + s, rgb = 0;
        if (sc_pad(pad) < 0)
            continue;
        if (sc_active())
            rgb = tpat(trk)->scale == BAR_SCALE_KEY[s] ? 0xffffff : 0x00301a;
        if (rgb != sg_led[SG_N + s]) {
            sg_led[SG_N + s] = rgb;
            host->led(pad, rgb >> 16, rgb >> 8 & 0xffu, rgb & 0xffu);
        }
    }
}

static void apply(const struct pp_map *m, int turn, int32_t d, uint32_t state)
{
    int mod = m->mod ? (int)m->mod - 1 : -1;
    if (m->t == MT_NONE)
        return;
    if (turn) {                                     /* potard ou encodeur : d crans */
        if (m->t == MT_ENC) {
            int role = m->id;
            if (mod >= 0) {
                mod_bit = 1u << mod;
                mod_until = host->millis() + MOD_HOLD_MS;
            }
            /* Liste SEL ouverte (lst de pages.c, lu ici sans toucher à LOOM) : PRESETS y déplacerait le
             * curseur comme SELECT ; l'encodeur du Play+ y fait ALGORITHM (changer la valeur), puisque
             * ALGORITHM n'a plus de rotation dans la matrice (pads 1 à 8). LOOM affiche toujours « ALGO
             * change » : c'est bien ALGORITHM qu'il reçoit. */
            if (role == EN_PRESET && lst.open)
                role = EN_ALGO;
            in_enc[role] += d;
        } else if (m->t == MT_MASTER) {
            in_master = (uint32_t)clamp((int32_t)in_master + 32 * d, 0, 4096);
        }
        return;
    }
    if (state == 2)
        return;                                     /* « tenu » : rien de neuf */
    if (m->t == MT_BTN) {
        if (state == 0 && mod >= 0)
            press(&in_btn, &rel_btn, mod, 0);
        press(&in_btn, &rel_btn, m->id, state);
        if (state == 1 && mod >= 0)
            press(&in_btn, &rel_btn, mod, 1);
    } else if (m->t == MT_KEY) {
        sg_push(2u, m->id, state);                  /* traité dans tick (sg_run) : notes directes sur Steps */
    } else if (m->t == MT_ENC && state == 0) {
        if (mod >= 0) {
            mod_bit = 1u << mod;
            mod_until = host->millis() + MOD_HOLD_MS;
        }
        if (m->act == MA_INC)
            in_enc[m->id] += 1;
        else if (m->act == MA_DEC)
            in_enc[m->id] -= 1;
        else if (m->act == MA_VAL && m->id == EN_ALGO && !lst.open)
            algo_value(m->val);                     /* (liste ouverte : ce serait N crans de valeur, pas la piste N) */
    } else if (m->t == MT_MASTER && state == 0) {
        in_master = (uint32_t)clamp((int32_t)in_master + (m->act == MA_DEC ? -256 : 256), 0, 4096);
    }
}

/* l'entrée de la couche Shift si elle existe et Shift est tenu, sinon celle de la couche normale */
#define PICK(TAB, i) (pp_shift && TAB[1][i].t != MT_NONE ? &TAB[1][i] : &TAB[0][i])

static void event(unsigned int kind, unsigned int a, unsigned int b)
{
    if (kind == PGI_EV_BUTTON && a == 10u) {        /* Shift du Play+ : la couche, rien d'autre */
        if (b != 2u)
            pp_shift = (b == 0u);
        return;
    }
    if (kind == PGI_EV_BUTTON && a < 12u)
        apply(PICK(MAP_BTN, a), 0, 0, b);
    else if (kind == PGI_EV_PAD && a < 160u && sc_pad(a) >= 0) {
        if (b == 0u)
            sg_push(3u, (uint32_t)sc_pad(a), 0u);
    } else if (kind == PGI_EV_PAD && a < 160u && sg_pad(a) >= 0) {
        if (b != 2u)
            sg_push(1u, (uint32_t)sg_pad(a), b);
    } else if (kind == PGI_EV_PAD && a < 160u)
        apply(PICK(MAP_PAD, a), 0, 0, b);
    else if (kind == PGI_EV_KNOB && a < 15u)
        apply(PICK(MAP_KNOB, a), 1, (int32_t)b, 0);
    else if (kind == PGI_EV_ENCODER)
        apply(PICK(MAP_ENC, 0), 1, (int32_t)b, 0);
}

/* ---- LED des pads ---------------------------------------------------------------------------------
 * Un pad relié à un bouton ou une touche du FM-1 suit sa lumière (allumé, moyen, faible, éteint) ; un pad
 * « valeur N » d'ALGORITHM est allumé quand la piste N est choisie ; un pad cran +/- reste faible. Couleur :
 * celle choisie dans la page, sinon blanc chaud (touches blanches), orange (noires), bleu (boutons), vert
 * (pistes), cyan (crans). Dessinés quand une lumière ou la piste change. */
static uint32_t lt_btn, lt_keys, lt_dimb, lt_dimk, lt_midb, lt_midk, lt_dirty = 1, lt_trk = 0xff, lt_lst;
void plat_lights(uint32_t buttons, uint32_t keys, uint32_t dim_buttons, uint32_t dim_keys, int glow)
{
    (void)glow;
    if (buttons != lt_btn || keys != lt_keys || dim_buttons != lt_dimb || dim_keys != lt_dimk)
        lt_dirty = 1;
    lt_btn = buttons, lt_keys = keys, lt_dimb = dim_buttons, lt_dimk = dim_keys;
}
void plat_lights_mid(uint32_t buttons, uint32_t keys)
{
    if (buttons != lt_midb || keys != lt_midk)
        lt_dirty = 1;
    lt_midb = buttons, lt_midk = keys;
}

static const uint8_t white_keys[16] = PLAT_WHITE_KEYS;

static void led_level(uint32_t pad, uint32_t l, const uint8_t *rgb)
{
    if (MAP_PAD_RGB[pad][3])
        rgb = MAP_PAD_RGB[pad];
    host->led(pad, rgb[0] * l / 255u, rgb[1] * l / 255u, rgb[2] * l / 255u);
}

static uint32_t level(uint32_t m, uint32_t on, uint32_t mid, uint32_t dim)
{
    return (on & m) ? 255u : (mid & m) ? 110u : (dim & m) ? 40u : 10u;
}

static void leds_update(void)
{
    static const uint8_t c_white[3] = {255, 220, 180}, c_black[3] = {255, 120, 20}, c_btn[3] = {80, 160, 255},
                         c_trk[3] = {60, 255, 90}, c_enc[3] = {40, 200, 255}, c_off[3] = {0, 0, 0};
    uint32_t i;
    if (proj.trk_sel != lt_trk || lst.open != lt_lst) {
        lt_trk = proj.trk_sel;
        lt_lst = lst.open;                          /* liste SEL : pads ALGO éteints (et coupés, apply) */
        lt_dirty = 1;
    }
    if (!lt_dirty)
        return;
    lt_dirty = 0;
    for (i = 0; i < 160u; i++) {
        const struct pp_map *m = &MAP_PAD[0][i];
        if (m->t == MT_KEY) {
            uint32_t k = m->id, w = 0, j;
            for (j = 0; j < 16u; j++)
                w |= white_keys[j] == k;
            led_level(i, level(1u << k, lt_keys, lt_midk, lt_dimk), w ? c_white : c_black);
        } else if (m->t == MT_BTN) {
            led_level(i, level(1u << m->id, lt_btn, lt_midb, lt_dimb), c_btn);
        } else if (m->t == MT_ENC && m->act == MA_VAL && m->id == EN_ALGO) {
            if (lst.open)
                led_level(i, 0u, c_off);
            else
                led_level(i, algo_index(proj.trk_sel) == (int)m->val - 1 ? 255u : 30u, c_trk);
        } else if (m->t == MT_ENC) {
            led_level(i, 40u, c_enc);
        } else if (sg_pad(i) < 0 && sc_pad(i) < 0) {   /* (les pads de pas et SCALE : sg_leds) */
            led_level(i, MAP_PAD_RGB[i][3] ? 40u : 0u, c_off);
        }
    }
}
void plat_leds(uint32_t buttons, uint32_t keys) { plat_lights(buttons, keys, 0, 0, 0); }
void plat_glow(int on) { (void)on; }
void plat_play_red(int red) { (void)red; }
void plat_dim_ns(uint32_t ns) { (void)ns; }

/* ---- temps, charge ---------------------------------------------------------------------------- */
uint32_t plat_ms(void) { return host->millis(); }
static volatile uint32_t cpu_pct;
uint32_t plat_cpu_pct(void) { return cpu_pct; }
uint32_t plat_cpu_khz(void) { return 600000u; }
uint32_t plat_xruns(void) { return 0; }
int plat_battery(void) { return 4; }                 /* « alimenté » : le Play+ n'a pas de batterie à montrer */

/* ---- second cœur : absent, LOOM fait tout sur le premier --------------------------------------- */
int plat_cpu2_set(int on) { (void)on; return -1; }
int plat_cpu2_on(void) { return 0; }
int plat_cpu2_run(void (*fn)(uint32_t), uint32_t arg) { (void)fn; (void)arg; return 0; }
int plat_cpu2_wait(void) { return -1; }
int plat_cpu2_busy(void) { return 0; }
int plat_cpu2_barred(void) { return 1; }
void plat_cpu2_sync(void) {}

/* ---- Bluetooth, USB, MIDI : absents ----------------------------------------------------------- */
int plat_ble_state(void) { return 0; }
int plat_ble_connected(void) { return 0; }
int plat_ble_linked(void) { return 0; }
int plat_ble_device(void) { return 0; }
void plat_ble_disconnect(void) {}
void plat_ble_enable(int on) { (void)on; }
int plat_ble_passkey(void) { return -1; }
void plat_ble_scan(int on) { (void)on; }
int plat_ble_found(int i, char *name, int *rssi) { (void)i; (void)name; (void)rssi; return 0; }
int plat_ble_connect(int i) { (void)i; return -1; }
int plat_midi_in(uint32_t *pkt, int *port) { (void)pkt; (void)port; return 0; }
void plat_midi_send(uint32_t pkt, uint32_t to) { (void)pkt; (void)to; }
void plat_usb_audio_full(int on) { (void)on; }
void plat_usb_host(int on) { (void)on; }
int plat_usb_host_msg(void) { return 0; }
int plat_usb_lost(void) { return 0; }

/* ---- stockage : les objets d'fm1-base (projets, préréglages, réglages) en fichiers sur la carte -------
 * Un fichier par objet, /Guests/LOOM-8/oNN.bin (pgi_host.file_*, ABI 3), lus tous au démarrage dans une
 * copie en RAM (24 x 3840 o) ; chaque sauvegarde met la copie à jour et réécrit le fichier. Pas de
 * « réserve » (plat_store_room -1, comme le simulateur d'fm1-base) ni de copie précédente. Sans services
 * fichier (hôte ABI < 3) : tout reste en RAM. */
static uint8_t st_data[OBJ_NOBJ][PLAT_STORE_MAX];
static int16_t st_len[OBJ_NOBJ];                  /* -1 : absent */
static uint32_t st_writes, st_fails;

static void st_name(char *n, uint32_t obj)
{
    n[0] = 'o';
    n[1] = (char)('0' + obj / 10u);
    n[2] = (char)('0' + obj % 10u);
    n[3] = '.', n[4] = 'b', n[5] = 'i', n[6] = 'n', n[7] = 0;
}

static void st_boot(void)
{
    uint32_t o;
    char n[8];
    for (o = 0; o < OBJ_NOBJ; o++) {
        int r = -1;
        st_name(n, o);
        if (host->abi >= 3 && host->file_read)
            r = host->file_read(n, st_data[o], PLAT_STORE_MAX);
        st_len[o] = (int16_t)(r > 0 ? r : -1);    /* vide = absent, comme storage.c */
    }
}

int plat_store_load(uint32_t obj, void *dst, uint32_t max)
{
    int n;
    if (obj >= OBJ_NOBJ || (n = st_len[obj]) <= 0)
        return -1;
    if ((uint32_t)n > max)
        n = (int)max;
    memcpy(dst, st_data[obj], (size_t)n);
    return n;
}
int plat_store_head(uint32_t obj, void *dst, uint32_t n)
{
    if (obj >= OBJ_NOBJ || st_len[obj] <= 0 || (uint32_t)st_len[obj] < n)
        return -1;
    memcpy(dst, st_data[obj], n);
    return (int)n;
}
int plat_store_size(uint32_t obj) { return obj < OBJ_NOBJ && st_len[obj] > 0 ? st_len[obj] : -1; }
int plat_store_save(uint32_t obj, const void *src, uint32_t len)
{
    char n[8];
    if (obj >= OBJ_NOBJ || len > PLAT_STORE_MAX)
        return -1;
    memcpy(st_data[obj], src, len);
    st_len[obj] = (int16_t)(len ? (int)len : -1);
    if (host->abi >= 3 && host->file_write) {
        st_name(n, obj);
        if (host->file_write(n, src, len) != 0) {
            st_fails++;
            return -7;                              /* « écrit mais pas relu » : LOOM affiche une erreur */
        }
    }
    st_writes++;
    return 0;
}
int plat_store_room(void) { return -1; }
const uint8_t *plat_data(uint32_t off) { (void)off; return 0; }

/* ---- ressources : res.bin de LOOM-8, intégré par res_blob.S ----------------------------------- */
extern const uint8_t loom_res[], loom_res_end[];
const uint8_t *plat_res(uint32_t *len)
{
    *len = (uint32_t)(loom_res_end - loom_res);
    return loom_res;
}

/* ---- boucle et son ---------------------------------------------------------------------------- */
static uint32_t last_ms, last_frame;

static void init(const struct pgi_host *h)
{
    host = h;
    /* Accélération des encodeurs de LOOM (ui.c, accel : x2 à x6 si deux crans arrivent à moins de 60 ms),
     * pensée pour les encodeurs du FM-1. Les potards du Play+ donnent 64 crans par tour (pilote de Polyend
     * 0x600af43c) et les envoient serrés : un geste rapide sautait de 6 en 6. accel_off est une variable de
     * ui.c (inclus ici par app.c) que LOOM ne remet à 0 que dans son outil de capture d'écrans (absent
     * ici) : on la met à 1 sans modifier LOOM. Retirer cette ligne pour retrouver l'accélération. */
    accel_off = 1;
    for (uint32_t i = 0; i < SG_N + 9u; i++)
        sg_led[i] = 0xff000000u;                    /* (sg_leds : tout à écrire la première fois) */
    st_boot();                                      /* les objets sauvegardés, avant que l'app ne les lise */
    palette_set(APP_PALETTE);
    app_init();
    lcd_fill(0, 0, 240, 240, 0);
#ifdef APP_BOOT
    app_boot();
#endif
    app_ui_init();
    last_ms = last_frame = host->millis();
}

static void master_text(void);
static void tick(void)
{
    uint32_t now = host->millis();
    if (now == last_ms)
        return;
    last_ms = now;
    sg_run();
#ifdef APP_LOOP
    app_loop();
#endif
    if (now - last_frame >= 16u) {
        last_frame = now;
        app_frame();
    } else {
        app_input_only();
    }
    if (pp_d0 < pp_d1 && host->abi >= 2 && host->screen) {
        host->screen(pp_fb, pp_d0, pp_d1, 1);
        pp_d0 = 240, pp_d1 = 0;
    }
    leds_update();
    sg_leds();
    master_text();
}

static int32_t blk[2 * APP_BLOCK];
static uint32_t blk_pos = APP_BLOCK;                /* trames déjà données du bloc courant */

static void audio(short *l, short *r, unsigned int n)
{
    unsigned int i;
    for (i = 0; i < n; i++) {
        int32_t a, b;
        if (blk_pos >= APP_BLOCK) {
            uint32_t t0 = fm1_ticks();
            app_render(blk, APP_BLOCK, APP_GAIN(in_master));
            cpu_pct = (fm1_ticks() - t0) / (APP_BLOCK * 600000000u / 44100u / 100u);   /* % d'un bloc */
            blk_pos = 0;
        }
        a = blk[2 * blk_pos] >> 8;
        b = blk[2 * blk_pos + 1] >> 8;
        l[i] = (short)clamp(a, -32768, 32767);
        r[i] = (short)clamp(b, -32768, 32767);
        blk_pos++;
    }
}

#ifdef APP_TICK
#error "LOOM-8 sans APP_TICK : à ajouter (1 kHz) si une version l'utilise"
#endif

/* bandes latérales (ABI 4) : quel potard du Play+ tient quel encodeur du FM-1 (table provisoire, voir
 * map_knob), et le volume MASTER en % */
static char master_txt[6] = "70%";
static const char *bands[16] = {"Pot1", "K1", "Pot2", "K2", "Pot3", "K3", "Pot4", "K4",
                                "Pot5", "ALGO", "Pot6", "PRST", "Pot7", master_txt, "", ""};

static void master_text(void)
{
    uint32_t p = in_master * 100u / 4096u;
    char *q = master_txt;
    if (p >= 100)
        *q++ = '1', p -= 100, *q++ = '0';
    if (p >= 10 || q != master_txt)
        *q++ = (char)('0' + p / 10u);
    *q++ = (char)('0' + p % 10u);
    *q++ = '%';
    *q = 0;
}

PGI_GUEST_BANDS("LOOM-8", init, audio, event, tick, bands);
