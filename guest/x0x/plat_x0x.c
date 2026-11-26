/* X0X (charlesvestal/fm1-x0x, GPL-3.0 ; boîte à rythmes 909 / 808 / 303 / breaks du FM-1, dans la famille Felucca)
 * en invité PGI sur le Play+ (Avrek OS). Code à nous (AvrekStudio) ; on ne touche à rien dans X0X.
 *
 * X0X a une vraie couche plateforme (firmware/src/app/plat.h) et un simulateur PC qui fait tourner toute l'app contre
 * elle (host/x0x_host.c) : on fait la même chose contre le Play+, comme pour FM1 Tonic (son dérivé, voir
 * guest/tonic/plat_tonic.c, dont ce fichier reprend la structure) :
 *   - écran : lcd_fill / lcd_blit de gfx.c dans pp_fb, 240 x 240, les rangées touchées vers pgi_host.screen ;
 *   - son : engine_render par blocs de 256 trames (24 bits), engine_volume, engine_load (main_fm1.c), en int16 ;
 *   - boucle (main_fm1.c) : ui_frame toutes les 16 ms, ui_input_only entre deux, engine_brk_loops au changement
 *     de pattern de BREAK ;
 *   - commandes : la matrice docs/correspondance/x0x.json (gen_map.py : mêmes noms FM-1), ou /Guests/X0X/map.json ;
 *   - LED : plat_leds / plat_keys_dim -> LED des pads reliés ;
 *   - mémoire : objets (réglages, son, patterns, song, motion) en fichiers dans /Guests/X0X/ ;
 *   - un seul cœur ; pas d'USB, de MIDI ni de samples utilisateur (emplacements vides).
 * Licence : GPL-3.0 ; le .pgi est pour un usage personnel, ne pas le distribuer. */
#include <stdint.h>
#include <string.h>
#include "pgi_guest.h"

static const struct pgi_host *host;

/* ---- écran : 240 x 240 RGB565 octets permutés (comme x0x_host.c) ------------------------------------ */
static uint16_t pp_fb[240 * 240];
static uint32_t pp_d0 = 240, pp_d1 = 0;         /* rangées touchées depuis le dernier envoi [d0, d1) */
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
static inline int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
#include "gfx.c"

#include "app/plat.h"

/* ---- commandes : la matrice (map_playplus.h, généré par guest/loom/gen_map.py) ------------------------
 * Deux couches : sans et avec Shift du Play+ (B10, jamais transmis). Un appui bref compte au moins une fois
 * (relâché gardé jusqu'à la lecture suivante) ; une combinaison « C:HOME+SELECT » tient le bouton du FM-1
 * pendant l'action (MOD_HOLD_MS après le dernier cran). Les potards du Play+ donnent des crans ; MASTER est
 * un potentiomètre sur le FM-1 : 32 / 4096 par cran. */
#include "map_playplus.h"
static uint32_t key_down_ms[NKEYS], key_len_ms[NKEYS];   /* (plat_key_tap_ms) */
#define MOD_HOLD_MS 250u
#define DWT_CYCCNT (*(volatile uint32_t *)0xE0001004u)   /* compteur de cycles du M7 (600 MHz) */

static volatile uint32_t in_btn, in_keys, rel_btn, rel_keys;
static volatile int32_t in_enc[NE];
static volatile uint32_t in_master = 2896;      /* ~ -3 dB ; MASTER Q12 */
static volatile uint32_t pp_shift, mod_bit, mod_until;

uint32_t plat_ms(void) { return host->millis(); }
uint32_t plat_buttons(void)
{
    uint32_t b = in_btn;
    in_btn &= ~rel_btn;
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
    return k & ((1u << NKEYS) - 1u);
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
int plat_battery(void) { return 4; }            /* « alimenté » : pas de batterie à montrer */

static void press(volatile uint32_t *held, volatile uint32_t *rel, int bit, uint32_t state)
{
    if (bit < 0)
        return;
    if (state == 0) {
        *held |= 1u << bit;
        *rel &= ~(1u << bit);
    } else if (state == 1) {
        *rel |= 1u << bit;
    }
}

static void apply(const struct pp_map *m, int turn, int32_t d, uint32_t state)
{
    int mod = m->mod ? (int)m->mod - 1 : -1;
    if (m->t == MT_NONE)
        return;
    if (turn) {
        if (m->t == MT_ENC) {
            if (mod >= 0) {
                mod_bit = 1u << mod;
                mod_until = host->millis() + MOD_HOLD_MS;
            }
            in_enc[m->id] += d;
        } else if (m->t == MT_MASTER) {
            in_master = (uint32_t)clamp((int32_t)in_master + 32 * d, 0, 4096);
        } else if ((m->t == MT_BTN || m->t == MT_KEY) && d > 0) {
            /* un bouton ou une touche sur un potard : un appui par cran vers la droite */
            if (m->t == MT_BTN)
                press(&in_btn, &rel_btn, m->id, 0), press(&in_btn, &rel_btn, m->id, 1);
            else
                press(&in_keys, &rel_keys, m->id, 0), press(&in_keys, &rel_keys, m->id, 1);
        }
        return;
    }
    if (state == 2)
        return;
    if (m->t == MT_BTN) {
        if (state == 0 && mod >= 0)
            press(&in_btn, &rel_btn, mod, 0);
        press(&in_btn, &rel_btn, m->id, state);
        if (state == 1 && mod >= 0)
            press(&in_btn, &rel_btn, mod, 1);
    } else if (m->t == MT_KEY) {
        if (m->id < NKEYS && state == 0)
            key_down_ms[m->id] = host->millis() | 1u;
        else if (m->id < NKEYS && state == 1 && key_down_ms[m->id])
            key_len_ms[m->id] = host->millis() - key_down_ms[m->id], key_down_ms[m->id] = 0;
        press(&in_keys, &rel_keys, m->id, state);
    } else if (m->t == MT_ENC && state == 0) {
        if (mod >= 0) {
            mod_bit = 1u << mod;
            mod_until = host->millis() + MOD_HOLD_MS;
        }
        in_enc[m->id] += m->act == MA_DEC ? -1 : m->act == MA_VAL ? (int32_t)m->val : 1;
    } else if (m->t == MT_MASTER && state == 0) {
        in_master = (uint32_t)clamp((int32_t)in_master + (m->act == MA_DEC ? -256 : 256), 0, 4096);
    }
}

#define PICK(TAB, i) (pp_shift && TAB[1][i].t != MT_NONE ? &TAB[1][i] : &TAB[0][i])

static void event(unsigned int kind, unsigned int a, unsigned int b)
{
    if (kind == PGI_EV_BUTTON && a == 10u) {    /* Shift du Play+ : la couche, rien d'autre */
        if (b != 2u)
            pp_shift = (b == 0u);
        return;
    }
    if (kind == PGI_EV_BUTTON && a < 12u)
        apply(PICK(MAP_BTN, a), 0, 0, b);
    else if (kind == PGI_EV_PAD && a < 160u)
        apply(PICK(MAP_PAD, a), 0, 0, b);
    else if (kind == PGI_EV_KNOB && a < 15u)
        apply(PICK(MAP_KNOB, a), 1, (int32_t)b, 0);
    else if (kind == PGI_EV_ENCODER)
        apply(PICK(MAP_ENC, 0), 1, (int32_t)b, 0);
}

/* ---- LED des pads ------------------------------------------------------------------------------------
 * Un pad relié à une touche ou un bouton du FM-1 suit sa lumière dans Tonic : allumé (plat_leds), faible
 * (plat_keys_dim : les pas posés d'une autre page...), très faible (plat_keys_faint), sinon un fond. Couleur :
 * celle de la matrice (pad_colors), sinon blanc chaud (touches blanches = pas), bleu (noires = pistes),
 * cyan (boutons). Écrit seulement quand une lumière change. */
static uint32_t lt_btn, lt_keys, lt_dim, lt_faint, lt_glow = 1, lt_dirty = 1;
void plat_leds(uint32_t buttons, uint32_t keys)
{
    if (buttons != lt_btn || keys != lt_keys)
        lt_dirty = 1;
    lt_btn = buttons, lt_keys = keys;
}
void plat_glow(int on)
{
    if ((uint32_t)(on != 0) != lt_glow)
        lt_dirty = 1;
    lt_glow = on != 0;
}
void plat_keys_dim(uint32_t keys)
{
    if (keys != lt_dim)
        lt_dirty = 1;
    lt_dim = keys;
}
static void plat_keys_faint(uint32_t keys)
{
    if (keys != lt_faint)
        lt_dirty = 1;
    lt_faint = keys;
}
void plat_play_red(int red) { (void)red; }

static const uint8_t white_key[NKEYS] = {1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1};
static uint32_t led_cache[160];

static void led_set(uint32_t pad, uint32_t l, const uint8_t *rgb)
{
    uint32_t v;
    if (MAP_PAD_RGB[pad][3])
        rgb = MAP_PAD_RGB[pad];
    v = (rgb[0] * l / 255u) << 16 | (rgb[1] * l / 255u) << 8 | (rgb[2] * l / 255u);
    if (v != led_cache[pad]) {
        led_cache[pad] = v;
        host->led(pad, v >> 16 & 0xffu, v >> 8 & 0xffu, v & 0xffu);
    }
}

static void leds_update(void)
{
    static const uint8_t c_white[3] = {255, 220, 180}, c_black[3] = {60, 130, 255}, c_btn[3] = {60, 200, 255},
                         c_off[3] = {0, 0, 0};
    uint32_t i;
    if (!lt_dirty)
        return;
    lt_dirty = 0;
    for (i = 0; i < 160u; i++) {
        const struct pp_map *m = &MAP_PAD[0][i];
        if (m->t == MT_KEY && m->id < NKEYS) {
            uint32_t bit = 1u << m->id;
            led_set(i, (lt_keys & bit) ? 255u : (lt_dim & bit) ? 90u : (lt_faint & bit) ? 35u : 12u,
                    white_key[m->id] ? c_white : c_black);
        } else if (m->t == MT_BTN) {
            led_set(i, (lt_btn >> m->id & 1u) ? 255u : lt_glow ? 30u : 8u, c_btn);
        } else if (m->t != MT_NONE) {
            led_set(i, 40u, c_btn);
        } else {
            led_set(i, MAP_PAD_RGB[i][3] ? 30u : 0u, c_off);
        }
    }
}

/* ---- MIDI, second cœur, mesures : absents ou simples ---------------------------------------------------- */
int plat_midi_in(uint32_t *pkt) { (void)pkt; return 0; }
void plat_midi_out(uint32_t pkt) { (void)pkt; }

static volatile uint32_t cpu_pct, cpu_peak;
uint32_t plat_cpu_pct(void) { return cpu_pct; }
uint32_t plat_xruns(void) { return 0; }
int plat_cpu2_set(int on) { (void)on; return -1; }   /* un seul cœur : Tonic rend tout sur le premier */
int plat_cpu2_on(void) { return 0; }
int plat_cpu2_run(void (*fn)(uint32_t), uint32_t arg) { (void)fn; (void)arg; return 0; }
int plat_cpu2_wait(void) { return -1; }
int plat_cpu2_gave_up(void) { return 0; }
uint32_t plat_cpu2_bad(void) { return 0; }
int plat_cpu2_barred(void) { return 0; }
int plat_vdc14_mv(void) { return 0; }
int plat_sysvdd_mv(void) { return 0; }
int plat_last_crash(uint32_t v[5]) { (void)v; return 0; }
void plat_perf_init(void) {}
uint32_t plat_cycles(void) { return DWT_CYCCNT; }
int plat_cycles_cpu(void) { return 1; }
uint32_t plat_cycles_hz(void) { return 600000000u; }
uint32_t plat_ticks24(void) { return DWT_CYCCNT / 25u; }
uint32_t plat_cpu_peak_pct(void)
{
    uint32_t p = cpu_peak;
    cpu_peak = 0;
    return p;
}
uint32_t plat_irq_stack_used(void) { return 0; }
uint32_t plat_irq_stack_size(void) { return 0; }
int plat_stalls(uint32_t s[3]) { (void)s; return -1; }
void plat_stalls_enable(int on) { (void)on; }

/* ---- samples utilisateur : aucun (comme le FM-1 depuis Tonic wip182) ------------------------------------ */
int plat_slot_zones(int k) { (void)k; return 0; }
const uint8_t *plat_slot(int k, int z, uint32_t *nsamples, uint32_t *rate, char name[9])
{
    (void)k, (void)z, (void)nsamples, (void)rate, (void)name;
    return 0;
}

/* ---- mémoire : fichiers dans /Guests/TONIC/ (pgi_host.file_*, ABI 3, boucle principale seulement) --------
 * Objets (OBJ_SET, OBJ_SOUND, OBJ_PAT0...) : oN.bin, copie en RAM lue au démarrage (7 x 3840 o), réécrite à
 * chaque sauvegarde ; la copie précédente (A/B du FM-1) devient oN.old (plat_store_load_prev). Emplacements de
 * projets (SAVE AS) : pNN.bin, et leur second endroit (notes, song, motion) xNN.bin, lus et écrits à la
 * demande. Sans services fichier (ABI < 3) : objets en RAM seulement, emplacements absents. */
static uint8_t st_data[OBJ_NOBJ][PLAT_STORE_MAX];
static int16_t st_len[OBJ_NOBJ];
static uint8_t st_tmp[PLAT_STORE_MAX];

static void fname(char *n, char c, uint32_t k, const char *ext)
{
    n[0] = c;
    n[1] = (char)('0' + k / 10u);
    n[2] = (char)('0' + k % 10u);
    n[3] = '.';
    n[4] = ext[0], n[5] = ext[1], n[6] = ext[2], n[7] = 0;
}
static int files(void) { return host->abi >= 3 && host->file_read && host->file_write; }
static int f_load(const char *n, void *dst, uint32_t max)
{
    int r;
    if (!files())
        return -1;
    r = host->file_read(n, st_tmp, PLAT_STORE_MAX);
    if (r <= 0)
        return -1;                              /* vide = absent, comme storage.c */
    if ((uint32_t)r > max)
        r = (int)max;
    memcpy(dst, st_tmp, (size_t)r);
    return r;
}
static int f_save(const char *n, const void *src, uint32_t len)
{
    if (!files())
        return -9;
    return host->file_write(n, src, len) == 0 ? 0 : -7;
}

static void st_boot(void)
{
    uint32_t o;
    char n[8];
    for (o = 0; o < OBJ_NOBJ; o++) {
        int r = -1;
        fname(n, 'o', o, "bin");
        if (files())
            r = host->file_read(n, st_data[o], PLAT_STORE_MAX);
        st_len[o] = (int16_t)(r > 0 ? r : -1);
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
int plat_store_save(uint32_t obj, const void *src, uint32_t len)
{
    char n[8];
    if (obj >= OBJ_NOBJ || len > PLAT_STORE_MAX)
        return -1;
    if (files() && st_len[obj] > 0) {
        fname(n, 'o', obj, "old");
        host->file_write(n, st_data[obj], (uint32_t)st_len[obj]);
    }
    memcpy(st_data[obj], src, len);
    st_len[obj] = (int16_t)(len ? (int)len : -1);
    if (!files())
        return 0;
    fname(n, 'o', obj, "bin");
    return f_save(n, src, len);
}
int plat_store_load_prev(uint32_t obj, void *dst, uint32_t max)
{
    char n[8];
    if (obj >= OBJ_NOBJ)
        return -1;
    fname(n, 'o', obj, "old");
    return f_load(n, dst, max);
}
/* touches : depuis quand une touche est tenue, ou l'était à son dernier relâché (plat.h) */
uint32_t plat_key_tap_ms(int k)
{
    if (k < 0 || k >= NKEYS)
        return 0;
    return key_down_ms[k] ? host->millis() - key_down_ms[k] : key_len_ms[k];
}
void plat_midi_out_main(uint32_t pkt) { (void)pkt; }
int plat_midi_out_room(void) { return 64; }

/* ---- l'app : comme x0x_host.c (lecteur d'écran sans personne pour l'écouter) ---------------------------- */
#include "app/x0x.h"
#define ANN_NAME "X0X " X0X_VERSION
#include "ann_words.h"
#include "announce.c"
static int ann_wire_send(const uint8_t *p, uint32_t n) { (void)p; (void)n; return -1; }
#include "app/project.c"
#include "app/ui.c"

/* ---- matrice sur la carte : /Guests/TONIC/map.json (même lecteur et mêmes noms que LOOM-8) ---------------- */
#include "mapjson.h"
static char map_txt[48 * 1024];
static char map_state[6] = "int";
static struct pp_map map_save_pad[2][160], map_save_btn[2][12], map_save_knob[2][15], map_save_enc[2][1];
static uint8_t map_save_rgb[160][4];
static const struct { const char *n; uint8_t id; } MJ_B[] = {
    {"B_FX", B_FX}, {"B_SEL", B_SEL}, {"B_ENV", B_ENV}, {"B_LFO", B_LFO}, {"B_EDIT", B_EDIT}, {"B_GLO", B_GLO},
    {"B_HOME", B_HOME}, {"B_SAVE", B_SAVE}, {"B_ARP", B_ARP}, {"B_SEQ", B_SEQ}, {"B_PLAY", B_PLAY},
    {"B_REC", B_REC}, {"B_OCTDN", B_OCTDN}, {"B_OCTUP", B_OCTUP},
    {"HOME", B_HOME}, {"REC", B_REC}, {"PLAY", B_PLAY}, {"SEQ", B_SEQ}, {"SAVE", B_SAVE}, {"OCT+", B_OCTUP},
    {"OCT-", B_OCTDN}, {"GLO", B_GLO}};
static const struct { const char *n; uint8_t id; } MJ_E[] = {
    {"EN_SELECT", EN_SELECT}, {"EN_ALGO", EN_ALGO}, {"EN_PRESET", EN_PRESET}, {"EN_K1", EN_K1}, {"EN_K2", EN_K2},
    {"EN_K3", EN_K3}, {"EN_K4", EN_K4}, {"SELECT", EN_SELECT}, {"PRESETS", EN_PRESET}, {"ALGORITHM", EN_ALGO},
    {"KNOB1", EN_K1}, {"KNOB2", EN_K2}, {"KNOB3", EN_K3}, {"KNOB4", EN_K4}};

static int mj_find(const char *s, uint32_t len, int enc)
{
    uint32_t i, j, n = enc ? sizeof MJ_E / sizeof MJ_E[0] : sizeof MJ_B / sizeof MJ_B[0];
    for (i = 0; i < n; i++) {
        const char *m = enc ? MJ_E[i].n : MJ_B[i].n;
        for (j = 0; j < len && m[j] == s[j]; j++)
            ;
        if (j == len && !m[len])
            return enc ? MJ_E[i].id : MJ_B[i].id;
    }
    return -1;
}

static int mj_link_tonic(const struct mj_link *l)
{
    struct pp_map e = {MT_NONE, 0, MA_DIRECT, 0, 0}, *slot;
    const char *f = l->fm1;
    uint32_t n = 0;
    int id;
    while (f[n])
        n++;
    if ((id = mj_find(f, n, 0)) >= 0 && f[0] == 'B')
        e.t = MT_BTN, e.id = (uint8_t)id;
    else if ((id = mj_find(f, n, 1)) >= 0 && f[0] == 'E')
        e.t = MT_ENC, e.id = (uint8_t)id;
    else if (mj_eq(f, "MASTER"))
        e.t = MT_MASTER;
    else if (f[0] == 'K' && f[1] == 'E' && f[2] == 'Y' && f[3] >= '0' && f[3] <= '9') {
        uint32_t k = 0, i;
        for (i = 3; f[i] >= '0' && f[i] <= '9'; i++)
            k = k * 10 + (uint32_t)(f[i] - '0');
        if (f[i] || k >= NKEYS)
            return 0;
        e.t = MT_KEY, e.id = (uint8_t)k;
    } else if (f[0] == 'C' && f[1] == ':') {    /* « C:HOME+SELECT » : le 1er « + » coupe (REC+OCT+) */
        uint32_t p = 2;
        int mod, w;
        while (f[p] && f[p] != '+')
            p++;
        if (!f[p] || !f[p + 1] || (mod = mj_find(f + 2, p - 2, 0)) < 0)
            return 0;
        if ((w = mj_find(f + p + 1, n - p - 1, 0)) >= 0)
            e.t = MT_BTN, e.id = (uint8_t)w;
        else if ((w = mj_find(f + p + 1, n - p - 1, 1)) >= 0)
            e.t = MT_ENC, e.id = (uint8_t)w;
        else
            return 0;
        e.mod = (uint8_t)(1 + mod);
    } else
        return 0;
    e.act = l->act == MJ_ACT_INC ? MA_INC : l->act == MJ_ACT_DEC ? MA_DEC : l->act == MJ_ACT_VAL ? MA_VAL : MA_DIRECT;
    e.val = (uint8_t)(l->act == MJ_ACT_VAL ? l->val : 0);
    if (l->kind == MJ_BTN && l->idx == 10u)
        return 0;                               /* (B10 : le Shift du Play+) */
    slot = l->kind == MJ_PAD ? &MAP_PAD[l->shift][l->idx] : l->kind == MJ_BTN ? &MAP_BTN[l->shift][l->idx]
         : l->kind == MJ_KNOB ? &MAP_KNOB[l->shift][l->idx] : &MAP_ENC[l->shift][0];
    if (slot->t != MT_NONE)
        return 0;
    *slot = e;
    return 1;
}
static void mj_color_tonic(uint32_t pad, uint32_t rgb)
{
    MAP_PAD_RGB[pad][0] = (uint8_t)(rgb >> 16), MAP_PAD_RGB[pad][1] = (uint8_t)(rgb >> 8);
    MAP_PAD_RGB[pad][2] = (uint8_t)rgb, MAP_PAD_RGB[pad][3] = 1;
}
static int mj_has(const char *t, uint32_t n, const char *w)
{
    uint32_t i, j;
    for (i = 0; i < n; i++) {
        for (j = 0; w[j] && i + j < n && t[i + j] == w[j]; j++)
            ;
        if (!w[j])
            return 1;
    }
    return 0;
}
static void map_load(void)
{
    int n, refused;
    if (!files())
        return;
    n = host->file_read("map.json", map_txt, sizeof map_txt);
    if (n <= 0)
        return;
    if ((uint32_t)n >= sizeof map_txt || !mj_has(map_txt, (uint32_t)n, "fm1-playplus-map/1")) {
        map_state[0] = 'e', map_state[1] = 'r', map_state[2] = 'r', map_state[3] = 0;
        return;
    }
    memcpy(map_save_pad, MAP_PAD, sizeof MAP_PAD), memcpy(map_save_btn, MAP_BTN, sizeof MAP_BTN);
    memcpy(map_save_knob, MAP_KNOB, sizeof MAP_KNOB), memcpy(map_save_enc, MAP_ENC, sizeof MAP_ENC);
    memcpy(map_save_rgb, MAP_PAD_RGB, sizeof MAP_PAD_RGB);
    memset(MAP_PAD, 0, sizeof MAP_PAD), memset(MAP_BTN, 0, sizeof MAP_BTN);
    memset(MAP_KNOB, 0, sizeof MAP_KNOB), memset(MAP_ENC, 0, sizeof MAP_ENC);
    memset(MAP_PAD_RGB, 0, sizeof MAP_PAD_RGB);
    if (mj_parse(map_txt, (uint32_t)n, mj_link_tonic, mj_color_tonic, &refused) <= 0) {
        memcpy(MAP_PAD, map_save_pad, sizeof MAP_PAD), memcpy(MAP_BTN, map_save_btn, sizeof MAP_BTN);
        memcpy(MAP_KNOB, map_save_knob, sizeof MAP_KNOB), memcpy(MAP_ENC, map_save_enc, sizeof MAP_ENC);
        memcpy(MAP_PAD_RGB, map_save_rgb, sizeof MAP_PAD_RGB);
        map_state[0] = 'e', map_state[1] = 'r', map_state[2] = 'r', map_state[3] = 0;
        return;
    }
    map_state[0] = 'S', map_state[1] = 'D', map_state[2] = refused ? '!' : 0, map_state[3] = 0;
}

/* ---- démarrage, boucle, son ------------------------------------------------------------------------------ */
static uint32_t last_ms, last_frame;
static uint8_t last_pat = 0xFF;
static volatile uint32_t ready;                 /* init finie : le son peut appeler le moteur */

static void init(const struct pgi_host *h)
{
    uint32_t i;
    host = h;
    map_load();
    st_boot();                                  /* les objets sauvegardés, avant que l'app ne les lise */
    for (i = 0; i < 160u; i++)
        led_cache[i] = 0xff000000u;
    /* le démarrage de main_fm1.c / x0x_host.c boot(), sans écran d'accueil (comme l'émulateur web) */
    engine_init(proj.pat, &proj.arr.song, proj.arr.lane, &proj.sound);
    if (project_load() != 0)
        project_defaults();
    engine_apply_sound(&proj.sound);
    seq.bpm = (float)proj.set.bpm_x10 / 10.0f;
    seq.accent_q7 = proj.set.accent_q7;
    project_apply_io();
    engine_brk_loops();
    lcd_fill(0, 0, 240, 240, C_BLACK);
    ui_init();
    last_ms = last_frame = h->millis();
    ready = 1;
}

static void tick(void)
{
    uint32_t now = host->millis();
    if (now == last_ms)
        return;
    last_ms = now;
    if (seq.ppat[TRK_BRK] != last_pat) {        /* le nouveau pattern de BREAK peut nommer d'autres boucles */
        last_pat = seq.ppat[TRK_BRK];
        engine_brk_loops();
    }
    if (now - last_frame >= 16u) {              /* ~60 images/s au plus, les commandes entre deux */
        last_frame = now;
        ui_frame();
    } else {
        ui_input_only();
    }
    if (pp_d0 < pp_d1 && host->abi >= 2 && host->screen) {
        host->screen(pp_fb, pp_d0, pp_d1, 1);
        pp_d0 = 240, pp_d1 = 0;
    }
    leds_update();
}

#define BLK 256u
static int32_t blk[2 * BLK];
static uint32_t blk_pos = BLK;

static void audio(short *l, short *r, unsigned int n)
{
    unsigned int i;
    if (!ready) {
        for (i = 0; i < n; i++)
            l[i] = r[i] = 0;
        return;
    }
    for (i = 0; i < n; i++) {
        if (blk_pos >= BLK) {
            uint32_t t0 = DWT_CYCCNT, pct;
            engine_render(blk, BLK);
            engine_volume(blk, BLK);
            pct = (DWT_CYCCNT - t0) / (BLK * 600000000u / 44100u / 100u);   /* % d'un bloc */
            cpu_pct = (cpu_pct * 15u + pct) / 16u;
            if (pct > cpu_peak)
                cpu_peak = pct;
            engine_load(pct, BLK);              /* la garde de surcharge de Tonic */
            blk_pos = 0;
        }
        l[i] = (short)clamp(blk[2 * blk_pos] >> 8, -32768, 32767);
        r[i] = (short)clamp(blk[2 * blk_pos + 1] >> 8, -32768, 32767);
        blk_pos++;
    }
}

/* bandes latérales (ABI 4) : qui fait quoi sur les potards, et la matrice (« int » compilée, « SD » carte) */
static const char *bands[16] = {"Pot1", "MASTR", "Pot2", "K1", "Pot3", "K2", "Pot4", "K3",
                                "Pot5", "K4", "Pot6", "ALGO", "Pot7", "PRST", "MAP", map_state};

PGI_GUEST_BANDS("X0X", init, audio, event, tick, bands);
