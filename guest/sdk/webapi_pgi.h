/* La moitié commune des invités dont le firmware a un émulateur web au format de Felucca (web/emu/*_web.c :
 * web_boot, web_render, web_buttons, web_keys, web_enc, web_master, web_screen, web_lit_*…) : Skiff, Melodee,
 * CoralFlower… sur le Play+ (Avrek OS). Code à nous (AvrekStudio).
 *
 * Leur émulateur web simule tout le FM-1 (l'horloge, l'interruption audio, la boucle principale, la flash en RAM) et
 * avance « tant que le son demandé n'existe pas » (web_render). On l'appelle donc depuis la BOUCLE du Play+, pour
 * garder une petite réserve de son (WA_AHEAD trames) qu'une file passe à l'interruption audio, qui ne fait que
 * recopier. L'interface du firmware tourne ainsi dans la boucle, jamais sous interruption.
 *
 * Le fichier plateforme inclut l'émulateur web du firmware (avec guest/sdk/hostshim) puis définit :
 *   WA_NAME, WA_BOOT(), WA_RENDER(n) (n <= 1024 trames dans WA_OUT_L / WA_OUT_R, flottants ±1), WA_SCREEN()
 *   (RGB565 240 x 240, octets dans l'ordre du panneau), WA_BUTTONS(étiquettes), WA_KEYS(masque 27 touches),
 *   WA_ENC(rôle, crans), WA_MASTER(0..1023), WA_LIT_BUTTONS() / WA_LIT_KEYS() / WA_DIM_BUTTONS() / WA_DIM_KEYS()
 *   (bit = étiquette B_* / touche), et les constantes B_* / EN_* / NB / NE ;
 *   facultatif, la flash gardée sur la carte : WA_NOR() / WA_NOR_SIZE() / WA_FLASH_WRITES() / WA_PLAYING() (lue
 *   avant WA_BOOT, réécrite dans /Guests/<NOM>/flash.bin quand elle a changé et que rien ne joue) ; WA_NOR_ERASE()
 *   si la flash simulée doit être effacée à la main ; WA_NOR_STAGE() / WA_NOR_STAGED(n) si le firmware la lit
 *   d'un tampon d'entrée à son démarrage (CoralFlower). */
#include "mapjson.h"
#ifndef B_SEL
#define B_SEL B_SCL
#endif
#include "map_playplus.h"
#define MOD_HOLD_MS 250u
#ifndef WA_AHEAD
#define WA_AHEAD 1536u                          /* ~35 ms de son d'avance */
#endif
#define WA_RING 4096u

static int16_t wa_ring[WA_RING * 2];
static volatile uint32_t wa_w, wa_r, wa_under;
static uint32_t fp_held, fp_rel, fp_keys, fp_krel, fp_shift, fp_mod, fp_mod_until;
static volatile uint32_t fp_ready;
static int32_t fp_master = 724;
static int32_t wa_enc_q[16];                    /* crans par rôle, passés au firmware dans la boucle */

static void fp_buttons_out(void)
{
    uint32_t lab = fp_held;
    if (fp_mod && (int32_t)(fp_mod_until - host->millis()) > 0)
        lab |= fp_mod;
    else
        fp_mod = 0;
    WA_BUTTONS(lab);
    WA_KEYS(fp_keys & ((1u << 27) - 1u));
}
static void fp_after_pass(void)
{
    fp_held &= ~fp_rel, fp_rel = 0;
    fp_keys &= ~fp_krel, fp_krel = 0;
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
    if (role < 16u)
        wa_enc_q[role] += n;
}
static void fp_master_set(int32_t k10)
{
    fp_master = k10 < 0 ? 0 : k10 > 1023 ? 1023 : k10;
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

/* ---- LED (bits par étiquette, comme les rend l'émulateur web) ---- */
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
    uint32_t lb = WA_LIT_BUTTONS(), lk = WA_LIT_KEYS(), db = WA_DIM_BUTTONS(), dk = WA_DIM_KEYS(), i;
    uint32_t s = lb * 31u + lk * 7u + db * 13u + dk * 17u + (lb ^ dk);
    if (s == fp_led_sum)
        return;
    fp_led_sum = s;
    for (i = 0; i < 160u; i++) {
        const struct pp_map *m = &MAP_PAD[0][i];
        if (m->t == MT_KEY && m->id < 27u)
            fp_led_set(i, (lk >> m->id & 1u) ? 255u : (dk >> m->id & 1u) ? 60u : 10u, fp_white[m->id] ? c_white : c_black);
        else if (m->t == MT_BTN && m->id < NB)
            fp_led_set(i, (lb >> m->id & 1u) ? 255u : (db >> m->id & 1u) ? 60u : 14u, c_btn);
        else if (m->t != MT_NONE)
            fp_led_set(i, 40u, c_btn);
        else
            fp_led_set(i, MAP_PAD_RGB[i][3] ? 30u : 0u, c_btn);
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

/* ---- démarrage, boucle, son ------------------------------------------------------------------------------ */
static uint32_t wa_last_ms, wa_writes, wa_dirty_ms;

static void wa_fill(uint32_t chunks)            /* la réserve de son, depuis la boucle : au plus `chunks` morceaux de
                                                 * 256 trames par appel, pour que la boucle (et les commandes) ne
                                                 * restent jamais prises ici si le firmware rend moins vite que le temps */
{
    while (chunks-- && wa_w - wa_r < WA_AHEAD) {
        uint32_t n = WA_AHEAD - (wa_w - wa_r), k;
        const float *L, *R;
        int32_t e;
        if (n > 256u)
            n = 256u;
        for (e = 0; e < 16; e++)                /* les crans en attente, avant ce morceau de temps */
            if (wa_enc_q[e]) {
                WA_ENC((uint32_t)e, wa_enc_q[e]);
                wa_enc_q[e] = 0;
            }
        WA_MASTER((uint32_t)fp_master);
        fp_buttons_out();
        WA_RENDER(n);
        fp_after_pass();
#ifdef WA_OUT_LR
        L = WA_OUT_LR, R = L + 1;               /* (gauche et droite entrelacées) */
#define WA_I(k) (2u * (k))
#else
        L = WA_OUT_L, R = WA_OUT_R;
#define WA_I(k) (k)
#endif
        for (k = 0; k < n; k++) {
            float a = L[WA_I(k)] * 32767.0f, b = R[WA_I(k)] * 32767.0f;
            uint32_t i = (wa_w + k) % WA_RING;
            wa_ring[2 * i] = (int16_t)(a > 32767.0f ? 32767 : a < -32768.0f ? -32768 : (int32_t)a);
            wa_ring[2 * i + 1] = (int16_t)(b > 32767.0f ? 32767 : b < -32768.0f ? -32768 : (int32_t)b);
        }
        __asm__ volatile("dmb" ::: "memory");
        wa_w += n;
    }
}

#ifdef WA_NOR
static void wa_nor_load(void)
{
    int n;
    if (!fp_files())
        return;
#ifdef WA_NOR_STAGE
    n = host->file_read("flash.bin", WA_NOR_STAGE(), WA_NOR_SIZE());   /* (lue par le firmware à son démarrage) */
    WA_NOR_STAGED(n > 0 ? (uint32_t)n : 0u);
#else
    n = host->file_read("flash.bin", WA_NOR(), WA_NOR_SIZE());
#endif
#ifdef WA_NOR_LOADED
    WA_NOR_LOADED(n > 0);
#endif
#ifdef WA_NOR_ERASE
    if (n <= 0)
        WA_NOR_ERASE();                         /* absent : une flash effacée, comme un FM-1 neuf */
#endif
}
static void wa_nor_save(void)
{
    if (fp_files())
        host->file_write("flash.bin", WA_NOR(), WA_NOR_SIZE());
}
#endif

static void fp_init(const struct pgi_host *h)
{
    uint32_t i;
    host = h;
    fp_map_load();
    for (i = 0; i < 160u; i++)
        fp_led_cache[i] = 0xff000000u;
#ifdef WA_NOR
#ifdef WA_NOR_ERASE
    if (!fp_files())
        WA_NOR_ERASE();
#endif
    wa_nor_load();
#endif
    WA_BOOT();
#ifdef WA_FLASH_WRITES
    wa_writes = WA_FLASH_WRITES();
#endif
    wa_fill(WA_AHEAD / 256u + 1u);
    wa_last_ms = h->millis();
    fp_ready = 1;
}

static void fp_tick(void)
{
    uint32_t now = host->millis();
    if (now == wa_last_ms)
        return;
    wa_last_ms = now;
    wa_fill(1);
    if (host->abi >= 2 && host->screen)
        host->screen(WA_SCREEN(), 0, 240, 1);   /* (l'hôte ne renvoie que les rangées dont le contenu a changé) */
    fp_leds();
#if defined(WA_NOR) && defined(WA_FLASH_WRITES)
    if (WA_FLASH_WRITES() != wa_writes) {       /* la flash a changé : sur la carte, 2 s après, à l'arrêt */
        wa_writes = WA_FLASH_WRITES();
        wa_dirty_ms = now | 1u;
    }
    if (wa_dirty_ms && now - wa_dirty_ms > 2000u
#ifdef WA_PLAYING
        && !WA_PLAYING()
#endif
    ) {
        wa_dirty_ms = 0;
        wa_nor_save();
#ifdef WA_FLASH_SAVED
        WA_FLASH_SAVED();
#endif
        wa_w = wa_r;                            /* (l'écriture a pu affamer le son : on repart d'une réserve neuve) */
    }
#endif
}

static void fp_audio(short *l, short *r, unsigned int n)
{
    unsigned int i;
    for (i = 0; i < n; i++) {
        if (fp_ready && wa_w - wa_r > 0) {
            uint32_t k = wa_r % WA_RING;
            l[i] = wa_ring[2 * k], r[i] = wa_ring[2 * k + 1];
            wa_r++;
        } else {
            l[i] = r[i] = 0;
            wa_under++;
        }
    }
}

static const char *fp_bands[16] = {"Pot1", "MASTR", "Pot2", "K1", "Pot3", "K2", "Pot4", "K3",
                                   "Pot5", "K4", "Pot6", "ALGO", "Pot7", "PRST", "MAP", fp_map_state};
PGI_GUEST_BANDS(WA_NAME, fp_init, fp_audio, fp_event, fp_tick, fp_bands);
