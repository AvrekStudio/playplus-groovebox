/* La moitié commune des invités « famille Felucca » (SLOOP, KARMA, ORBIT…) sur le Play+ (Avrek OS).
 * Code à nous (AvrekStudio). À inclure à la fin du fichier plateforme d'un firmware, une fois ses sources incluses
 * comme le fait son harnais de test (tests/ui_pages_test.c) ou son émulateur web : il faut alors
 *   fm1_in (.buttons = matrice des boutons, .notes = touches 0..26), edges_btn, encs[7], panel (btn[], enc[],
 *   dir[]), NB, NE, song.master_q12, fm1_ms, fm1_led / fm1_led_dim / fm1_led_bg, FM1_KEYMAP, FM1_NCOL,
 *   screen[240*240] (RGB565, octets permutés) (et scr_d0 / scr_d1, rangées touchées, si FP_SCR_TRACK),
 *   mix_block(int32_t *, CTL),
 *   ui_input(), ui_leds(), ui_draw() ;
 * et le fichier définit avant l'inclusion :
 *   FELUCCA_GUEST_NAME  nom de l'invité (« SLOOP » : dossier /Guests/SLOOP/) ;
 *   felucca_boot()      la mise sous tension du firmware (comme son main.c / son web_boot), avant le son ;
 *   facultatif : felucca_scope(o) après chaque bloc (l'oscilloscope du visualiseur), FELUCCA_OUT_SHIFT (décalage
 *   de mix_block vers 16 bits, défaut 0).
 * Ce que fait ce fichier, comme web/emu/sloop_web.c de KARMA (GPL-3.0) le fait pour le navigateur :
 *   - son : mix_block par blocs de CTL trames, sous l'interruption audio du Play+ ; le Play+ demande des paquets
 *     de taille quelconque : une petite file entre les deux ;
 *   - boucle : fm1_ms = l'horloge du Play+ ; ui_input / ui_leds / ui_draw toutes les 16 ms, ui_input entre deux
 *     (main.c du FM-1) ; les rangées touchées de `screen` envoyées par pgi_host.screen ;
 *   - commandes : la matrice (map_playplus.h, guest/loom/gen_map.py ; ou <dossier>/map.json, même lecteur que
 *     LOOM-8) vers les étiquettes du FM-1 (B_*, EN_*, touches, MASTER) puis vers sa matrice par `panel`. Un appui
 *     plus court qu'un passage de l'interface est gardé jusqu'au passage suivant (sinon ui_input ne le voit pas) ;
 *   - LED : fm1_led (allumée), fm1_led_dim / fm1_led_bg (faible) relues par la matrice de touches du FM-1, vers les
 *     pads reliés à une touche ou un bouton. */
#include "mapjson.h"

#ifndef B_SEL
#define B_SEL B_SCL                             /* (SLOOP nomme SCL le 2e bouton du haut ; les matrices, SEL) */
#endif
#include "map_playplus.h"

#ifndef FELUCCA_OUT_SHIFT
#define FELUCCA_OUT_SHIFT 0
#endif
#define MOD_HOLD_MS 250u
#define DWT_CYCCNT (*(volatile uint32_t *)0xE0001004u)

static uint32_t fp_held, fp_rel;                /* boutons par étiquette (bit = B_*) : tenus, relâchés à appliquer */
static uint32_t fp_keys, fp_krel;               /* touches 0..26 */
static uint32_t fp_shift, fp_mod, fp_mod_until;
static volatile uint32_t fp_ready;

static uint32_t fp_matrix(uint32_t labels)      /* étiquettes -> boutons de la matrice du FM-1 */
{
    uint32_t b, m = 0;
    for (b = 0; b < NB; b++)
        if ((labels >> b) & 1u)
            m |= 1u << panel.btn[b];
    return m;
}
static void fp_buttons_out(void)
{
    uint32_t lab = fp_held, m;
    if (fp_mod && (int32_t)(fp_mod_until - host->millis()) > 0)
        lab |= fp_mod;
    else
        fp_mod = 0;
    m = fp_matrix(lab);
    edges_btn |= m & ~fm1_in.buttons;
    fm1_in.buttons = m;
    fm1_in.notes = fp_keys & ((1u << 27) - 1u);
}
static void fp_after_pass(void)                 /* un passage de l'interface a vu les appuis : les relâchés partent */
{
    fp_held &= ~fp_rel, fp_rel = 0;
    fp_keys &= ~fp_krel, fp_krel = 0;
    fp_buttons_out();
}
static void fp_press(uint32_t *held, uint32_t *rel, int bit, uint32_t state)
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
static void fp_enc(uint32_t role, int32_t n)
{
    if (role < NE)
        encs[panel.enc[role] % 7u] += n * panel.dir[role];
}
static int32_t fp_master = 724;                 /* 0..1023 (main.c : le potentiomètre au carré) ; ~ -6 dB */
static void fp_master_set(int32_t k10)
{
    fp_master = k10 < 0 ? 0 : k10 > 1023 ? 1023 : k10;
    song.master_q12 = ((uint32_t)fp_master * (uint32_t)fp_master) >> 8;
}

static void fp_apply(const struct pp_map *m, int turn, int32_t d, uint32_t state)
{
    int mod = m->mod ? (int)m->mod - 1 : -1;
    if (m->t == MT_NONE)
        return;
    if (turn) {
        if (m->t == MT_ENC) {
            if (mod >= 0) {
                fp_mod = 1u << mod;
                fp_mod_until = host->millis() + MOD_HOLD_MS;
            }
            fp_enc(m->id, d);
        } else if (m->t == MT_MASTER) {
            fp_master_set(fp_master + 8 * d);
        } else if (m->t == MT_BTN && d > 0) {
            fp_press(&fp_held, &fp_rel, m->id, 0), fp_press(&fp_held, &fp_rel, m->id, 1);
        } else if (m->t == MT_KEY && d > 0) {
            fp_press(&fp_keys, &fp_krel, m->id, 0), fp_press(&fp_keys, &fp_krel, m->id, 1);
        }
    } else if (state != 2) {
        if (m->t == MT_BTN) {
            if (state == 0 && mod >= 0)
                fp_press(&fp_held, &fp_rel, mod, 0);
            fp_press(&fp_held, &fp_rel, m->id, state);
            if (state == 1 && mod >= 0)
                fp_press(&fp_held, &fp_rel, mod, 1);
        } else if (m->t == MT_KEY) {
            fp_press(&fp_keys, &fp_krel, m->id, state);
        } else if (m->t == MT_ENC && state == 0) {
            if (mod >= 0) {
                fp_mod = 1u << mod;
                fp_mod_until = host->millis() + MOD_HOLD_MS;
            }
            fp_enc(m->id, m->act == MA_DEC ? -1 : m->act == MA_VAL ? (int32_t)m->val : 1);
        } else if (m->t == MT_MASTER && state == 0) {
            fp_master_set(fp_master + (m->act == MA_DEC ? -64 : 64));
        }
    }
    fp_buttons_out();
}

#define FP_PICK(TAB, i) (fp_shift && TAB[1][i].t != MT_NONE ? &TAB[1][i] : &TAB[0][i])
static void fp_event(unsigned int kind, unsigned int a, unsigned int b)
{
    if (!fp_ready)
        return;
    if (kind == PGI_EV_BUTTON && a == 10u) {    /* Shift du Play+ : la couche, rien d'autre */
        if (b != 2u)
            fp_shift = (b == 0u);
        return;
    }
    if (kind == PGI_EV_BUTTON && a < 12u)
        fp_apply(FP_PICK(MAP_BTN, a), 0, 0, b);
    else if (kind == PGI_EV_PAD && a < 160u)
        fp_apply(FP_PICK(MAP_PAD, a), 0, 0, b);
    else if (kind == PGI_EV_KNOB && a < 15u)
        fp_apply(FP_PICK(MAP_KNOB, a), 1, (int32_t)b, 0);
    else if (kind == PGI_EV_ENCODER)
        fp_apply(FP_PICK(MAP_ENC, 0), 1, (int32_t)b, 0);
}

/* ---- LED -------------------------------------------------------------------------------------------- */
static const int8_t FP_KEYMAP[6][11] = {       /* hal/fm1_input.h : l'identifiant de touche en (colonne, bit) ; la
                                                 * copie de sloop_web.c (les harnais n'ont qu'un tableau vide) */
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 5, 11,  4, 10,  3,  9,  2,  8, -1, -1, -1},
    {34, 35, 36, 37, 38, 40, 39, 13,  7,  6, 12},
    {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
    { 0,  1, 15, 14, 17, 16, 19, 18, 20, 21, 22},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
};
static uint32_t fp_led_of(const uint8_t *l, uint32_t id)
{
    uint32_t c, r;
    for (c = 0; c < 11u; c++)
        for (r = 1; r < 5u; r++)
            if (FP_KEYMAP[r][c] == (int8_t)id)
                return (l[c] >> r) & 1u;
    return 0;
}
static const uint8_t fp_white[27] = {1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1};
static uint32_t fp_led_cache[160], fp_led_sum;
static void fp_led_set(uint32_t pad, uint32_t l, const uint8_t *rgb)
{
    uint32_t v;
    if (MAP_PAD_RGB[pad][3])
        rgb = MAP_PAD_RGB[pad];
    v = (rgb[0] * l / 255u) << 16 | (rgb[1] * l / 255u) << 8 | (rgb[2] * l / 255u);
    if (v != fp_led_cache[pad]) {
        fp_led_cache[pad] = v;
        host->led(pad, v >> 16 & 0xffu, v >> 8 & 0xffu, v & 0xffu);
    }
}
static void fp_leds(void)
{
    static const uint8_t c_white[3] = {255, 230, 200}, c_black[3] = {70, 130, 255}, c_btn[3] = {60, 200, 255};
    uint32_t i, s = 0;
    for (i = 0; i < 16u; i++)                   /* rien n'a changé dans les LED du FM-1 : rien à refaire */
        s = s * 31u + fm1_led[i] + 7u * fm1_led_dim[i] + 13u * fm1_led_bg[i];
    if (s == fp_led_sum)
        return;
    fp_led_sum = s;
    for (i = 0; i < 160u; i++) {
        const struct pp_map *m = &MAP_PAD[0][i];
        if (m->t == MT_KEY && m->id < 27u) {
            uint32_t id = 14u + m->id;
            fp_led_set(i, fp_led_of(fm1_led, id) ? 255u : (fp_led_of(fm1_led_dim, id) || fp_led_of(fm1_led_bg, id)) ? 60u
                          : 10u, fp_white[m->id] ? c_white : c_black);
        } else if (m->t == MT_BTN && m->id < NB) {
            uint32_t id = panel.btn[m->id];
            fp_led_set(i, fp_led_of(fm1_led, id) ? 255u : (fp_led_of(fm1_led_dim, id) || fp_led_of(fm1_led_bg, id)) ? 60u
                          : 14u, c_btn);
        } else if (m->t != MT_NONE) {
            fp_led_set(i, 40u, c_btn);
        } else {
            fp_led_set(i, MAP_PAD_RGB[i][3] ? 30u : 0u, c_btn);
        }
    }
}

/* ---- la matrice sur la carte : <dossier>/map.json (même lecteur et mêmes noms que LOOM-8) ----------------- */
static char fp_map_txt[48 * 1024];
static char fp_map_state[6] = "int";
static struct pp_map fp_sv_pad[2][160], fp_sv_btn[2][12], fp_sv_knob[2][15], fp_sv_enc[2][1];
static uint8_t fp_sv_rgb[160][4];
static const struct { const char *n; uint8_t id; } FP_B[] = {
    {"B_FX", B_FX}, {"B_SEL", B_SEL}, {"B_ENV", B_ENV}, {"B_LFO", B_LFO}, {"B_EDIT", B_EDIT}, {"B_GLO", B_GLO},
    {"B_HOME", B_HOME}, {"B_SAVE", B_SAVE}, {"B_ARP", B_ARP}, {"B_SEQ", B_SEQ}, {"B_PLAY", B_PLAY},
    {"B_REC", B_REC}, {"B_OCTDN", B_OCTDN}, {"B_OCTUP", B_OCTUP},
    {"FX", B_FX}, {"SEL", B_SEL}, {"ENV", B_ENV}, {"LFO", B_LFO}, {"EDIT", B_EDIT}, {"GLO", B_GLO}, {"HOME", B_HOME},
    {"SAVE", B_SAVE}, {"ARP", B_ARP}, {"SEQ", B_SEQ}, {"PLAY", B_PLAY}, {"REC", B_REC}, {"OCT-", B_OCTDN},
    {"OCT+", B_OCTUP}};
static const struct { const char *n; uint8_t id; } FP_E[] = {
    {"EN_SELECT", EN_SELECT}, {"EN_ALGO", EN_ALGO}, {"EN_PRESET", EN_PRESET}, {"EN_K1", EN_K1}, {"EN_K2", EN_K2},
    {"EN_K3", EN_K3}, {"EN_K4", EN_K4}, {"SELECT", EN_SELECT}, {"PRESETS", EN_PRESET}, {"ALGORITHM", EN_ALGO},
    {"KNOB1", EN_K1}, {"KNOB2", EN_K2}, {"KNOB3", EN_K3}, {"KNOB4", EN_K4}};
static int fp_find(const char *s, uint32_t len, int enc)
{
    uint32_t i, j, n = enc ? sizeof FP_E / sizeof FP_E[0] : sizeof FP_B / sizeof FP_B[0];
    for (i = 0; i < n; i++) {
        const char *m = enc ? FP_E[i].n : FP_B[i].n;
        for (j = 0; j < len && m[j] == s[j]; j++)
            ;
        if (j == len && !m[len])
            return enc ? FP_E[i].id : FP_B[i].id;
    }
    return -1;
}
static int fp_link(const struct mj_link *l)
{
    struct pp_map e = {MT_NONE, 0, MA_DIRECT, 0, 0}, *slot;
    const char *f = l->fm1;
    uint32_t n = 0;
    int id;
    while (f[n])
        n++;
    if (f[0] == 'B' && f[1] == '_' && (id = fp_find(f, n, 0)) >= 0)
        e.t = MT_BTN, e.id = (uint8_t)id;
    else if (f[0] == 'E' && f[1] == 'N' && (id = fp_find(f, n, 1)) >= 0)
        e.t = MT_ENC, e.id = (uint8_t)id;
    else if (mj_eq(f, "MASTER"))
        e.t = MT_MASTER;
    else if (f[0] == 'K' && f[1] == 'E' && f[2] == 'Y' && f[3] >= '0' && f[3] <= '9') {
        uint32_t k = 0, i;
        for (i = 3; f[i] >= '0' && f[i] <= '9'; i++)
            k = k * 10 + (uint32_t)(f[i] - '0');
        if (f[i] || k >= 27u)
            return 0;
        e.t = MT_KEY, e.id = (uint8_t)k;
    } else if (f[0] == 'C' && f[1] == ':') {    /* « C:EDIT+OCT- » : le 1er « + » après le 1er caractère coupe */
        uint32_t p = 3;
        int mod, w;
        while (f[p] && f[p] != '+')
            p++;
        if (!f[p] || !f[p + 1] || (mod = fp_find(f + 2, p - 2, 0)) < 0)
            return 0;
        if ((w = fp_find(f + p + 1, n - p - 1, 0)) >= 0)
            e.t = MT_BTN, e.id = (uint8_t)w;
        else if ((w = fp_find(f + p + 1, n - p - 1, 1)) >= 0)
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
static void fp_color(uint32_t pad, uint32_t rgb)
{
    MAP_PAD_RGB[pad][0] = (uint8_t)(rgb >> 16), MAP_PAD_RGB[pad][1] = (uint8_t)(rgb >> 8);
    MAP_PAD_RGB[pad][2] = (uint8_t)rgb, MAP_PAD_RGB[pad][3] = 1;
}
static int fp_files(void) { return host->abi >= 3 && host->file_read && host->file_write; }
static void fp_map_load(void)
{
    int n, refused;
    uint32_t i, j, ok = 0;
    static const char tag[] = "fm1-playplus-map/1";
    if (!fp_files())
        return;
    n = host->file_read("map.json", fp_map_txt, sizeof fp_map_txt);
    if (n <= 0)
        return;
    for (i = 0; !ok && i + sizeof tag - 1u <= (uint32_t)n; i++) {
        for (j = 0; tag[j] && fp_map_txt[i + j] == tag[j]; j++)
            ;
        ok = !tag[j];
    }
    if ((uint32_t)n >= sizeof fp_map_txt || !ok) {
        fp_map_state[0] = 'e', fp_map_state[1] = 'r', fp_map_state[2] = 'r', fp_map_state[3] = 0;
        return;
    }
    memcpy(fp_sv_pad, MAP_PAD, sizeof MAP_PAD), memcpy(fp_sv_btn, MAP_BTN, sizeof MAP_BTN);
    memcpy(fp_sv_knob, MAP_KNOB, sizeof MAP_KNOB), memcpy(fp_sv_enc, MAP_ENC, sizeof MAP_ENC);
    memcpy(fp_sv_rgb, MAP_PAD_RGB, sizeof MAP_PAD_RGB);
    memset(MAP_PAD, 0, sizeof MAP_PAD), memset(MAP_BTN, 0, sizeof MAP_BTN);
    memset(MAP_KNOB, 0, sizeof MAP_KNOB), memset(MAP_ENC, 0, sizeof MAP_ENC);
    memset(MAP_PAD_RGB, 0, sizeof MAP_PAD_RGB);
    if (mj_parse(fp_map_txt, (uint32_t)n, fp_link, fp_color, &refused) <= 0) {
        memcpy(MAP_PAD, fp_sv_pad, sizeof MAP_PAD), memcpy(MAP_BTN, fp_sv_btn, sizeof MAP_BTN);
        memcpy(MAP_KNOB, fp_sv_knob, sizeof MAP_KNOB), memcpy(MAP_ENC, fp_sv_enc, sizeof MAP_ENC);
        memcpy(MAP_PAD_RGB, fp_sv_rgb, sizeof MAP_PAD_RGB);
        fp_map_state[0] = 'e', fp_map_state[1] = 'r', fp_map_state[2] = 'r', fp_map_state[3] = 0;
        return;
    }
    fp_map_state[0] = 'S', fp_map_state[1] = 'D', fp_map_state[2] = refused ? '!' : 0, fp_map_state[3] = 0;
}

/* ---- démarrage, boucle, son -------------------------------------------------------------------------- */
static uint32_t fp_last_ms, fp_frame_ms;

static void fp_init(const struct pgi_host *h)
{
    uint32_t i;
    host = h;
    fm1_ms = h->millis();
    fp_map_load();
    for (i = 0; i < 160u; i++)
        fp_led_cache[i] = 0xff000000u;
    felucca_boot();
    fp_master_set(fp_master);
    fp_last_ms = fp_frame_ms = h->millis();
    fp_ready = 1;
}

static void fp_tick(void)
{
    uint32_t now = host->millis();
    if (now == fp_last_ms)
        return;
    fp_last_ms = now;
    fm1_ms = now;
    fp_buttons_out();
    if (now - fp_frame_ms >= 16u) {             /* ~60 passages par seconde (main.c), les commandes entre deux */
        fp_frame_ms = now;
        ui_input();
        ui_leds();
        ui_draw();
    } else {
        ui_input();
    }
    fp_after_pass();
#ifdef FP_SCR_TRACK
    if (scr_d0 < scr_d1 && host->abi >= 2 && host->screen) {
        host->screen(screen, scr_d0, scr_d1, 1);
        scr_d0 = 240, scr_d1 = 0;
    }
#else
    if (now == fp_frame_ms && host->abi >= 2 && host->screen)
        host->screen(screen, 0, 240, 1);        /* (l'hôte ne renvoie que les rangées dont le contenu a changé) */
#endif
    fp_leds();
}

static int32_t fp_blk[2 * CTL];
static uint32_t fp_pos = CTL;
static volatile uint32_t fp_cpu;
static void fp_audio(short *l, short *r, unsigned int n)
{
    unsigned int i;
    if (!fp_ready) {
        for (i = 0; i < n; i++)
            l[i] = r[i] = 0;
        return;
    }
    for (i = 0; i < n; i++) {
        if (fp_pos >= CTL) {
            uint32_t t0 = DWT_CYCCNT;
            mix_block(fp_blk, CTL);
#ifdef felucca_scope
            felucca_scope(fp_blk);
#endif
            fp_cpu = (fp_cpu * 15u + (DWT_CYCCNT - t0) / (CTL * 600000000u / FS / 100u)) / 16u;
            fp_pos = 0;
        }
        {
            int32_t a = fp_blk[2 * fp_pos] >> FELUCCA_OUT_SHIFT, b = fp_blk[2 * fp_pos + 1] >> FELUCCA_OUT_SHIFT;
            l[i] = (short)(a < -32768 ? -32768 : a > 32767 ? 32767 : a);
            r[i] = (short)(b < -32768 ? -32768 : b > 32767 ? 32767 : b);
        }
        fp_pos++;
    }
}

static const char *fp_bands[16] = {"Pot1", "MASTR", "Pot2", "K1", "Pot3", "K2", "Pot4", "K3",
                                   "Pot5", "K4", "Pot6", "ALGO", "Pot7", "PRST", "MAP", fp_map_state};
PGI_GUEST_BANDS(FELUCCA_GUEST_NAME, fp_init, fp_audio, fp_event, fp_tick, fp_bands);
