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
 *   - commandes : table de correspondance PROVISOIRE (machine pas encore en main), voir map_* plus bas ;
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

/* ---- commandes : correspondance Play+ -> FM-1 (provisoire) --------------------------------------
 * Boutons du Play+ (identifiants vus dans l'officiel) : 0 et 3 à gauche de l'écran, 6 clic de
 * l'encodeur, 7 Back, 8 Play, 9 bouton logiciel 3, 10 Shift. Shift + Back et Shift + clic restent au
 * mode invité (sortie, sélecteur).
 * Pads : grille 8 x 20 (index rangée x 20 + colonne). Colonnes 0..15 des rangées 6 (noires) et 7
 * (blanches) : le clavier de 27 touches (F3..G5) ; colonnes 16..19 des rangées 0..3 : les 14 boutons
 * du FM-1 (tous disponibles même sans bouton physique attribué).
 * Potards 0..3 : K1..K4 ; 4 : ALGO ; 5 : PRESET ; 6 : MASTER. Encodeur de l'écran : SELECT. */
static const int8_t map_button[16] = {B_FX, -1, -1, B_EDIT, -1, -1, B_GLO, B_HOME, B_PLAY, B_REC, B_SEL,
                                      -1, -1, -1, -1, -1};
static const int8_t map_fnpad[16] = {B_FX, B_ENV, B_LFO, B_EDIT, B_GLO, B_HOME, B_SAVE, B_ARP,
                                     B_SEQ, B_PLAY, B_REC, B_SEL, B_OCTDN, B_OCTUP, -1, -1};
static const int8_t map_knob[8] = {EN_K1, EN_K2, EN_K3, EN_K4, EN_ALGO, EN_PRESET, -2 /* MASTER */, -1};
static const uint8_t white_keys[16] = PLAT_WHITE_KEYS;
/* touche noire après la blanche w (F#, G#, A#, C#, D#...), -1 sinon (pas de noire après B et E) */
static const int8_t black_after[16] = {1, 3, 5, -1, 8, 10, -1, 13, 15, 17, -1, 20, 22, -1, 25, -1};

static volatile uint32_t in_btn, in_keys;          /* tenus */
static volatile uint32_t rel_btn, rel_keys;        /* relâchés pas encore vus : gardés un tour */
static volatile int32_t in_enc[NE];
static volatile uint32_t in_master = 2896;         /* ~ -6 dB (APP_GAIN : carré) */

uint32_t plat_buttons(void)
{
    uint32_t b = in_btn;
    in_btn &= ~rel_btn;                             /* un appui bref compte une fois au moins */
    rel_btn = 0;
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

static int pad_key(uint32_t pad)
{
    uint32_t row = pad / 20u, col = pad % 20u;
    if (col >= 16u)
        return -1;
    if (row == 7u)
        return white_keys[col];
    if (row == 6u)
        return black_after[col];
    return -1;
}

static void event(unsigned int kind, unsigned int a, unsigned int b)
{
    if (kind == PGI_EV_BUTTON && a < 16u) {
        press(&in_btn, &rel_btn, map_button[a], b);
    } else if (kind == PGI_EV_PAD && a < 160u) {
        uint32_t row = a / 20u, col = a % 20u;
        if (col >= 16u && row < 4u)
            press(&in_btn, &rel_btn, map_fnpad[row * 4u + (col - 16u)], b);
        else
            press(&in_keys, &rel_keys, pad_key(a), b);
    } else if (kind == PGI_EV_KNOB && a < 8u) {
        int m = map_knob[a];
        if (m >= 0)
            in_enc[m] += (int32_t)b;
        else if (m == -2)
            in_master = (uint32_t)clamp((int32_t)in_master + 64 * (int32_t)b, 0, 4096);
    } else if (kind == PGI_EV_ENCODER) {
        in_enc[EN_SELECT] += (int32_t)b;
    }
}

/* ---- LED ------------------------------------------------------------------------------------- */
static uint32_t lt_btn, lt_keys, lt_dimb, lt_dimk, lt_midb, lt_midk, lt_dirty = 1;
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

static void led_level(uint32_t pad, uint32_t on, uint32_t mid, uint32_t dim, const uint8_t *rgb)
{
    uint32_t l = on ? 255u : mid ? 110u : dim ? 40u : 10u;
    host->led(pad, rgb[0] * l / 255u, rgb[1] * l / 255u, rgb[2] * l / 255u);
}

static void leds_update(void)
{
    static const uint8_t c_white[3] = {255, 220, 180}, c_black[3] = {255, 120, 20}, c_btn[3] = {80, 160, 255};
    uint32_t i;
    if (!lt_dirty)
        return;
    lt_dirty = 0;
    for (i = 0; i < 16u; i++) {
        uint32_t k = white_keys[i], m = 1u << k;
        led_level(140u + i, lt_keys & m, lt_midk & m, lt_dimk & m, c_white);
        if (black_after[i] >= 0) {
            m = 1u << black_after[i];
            led_level(120u + i, lt_keys & m, lt_midk & m, lt_dimk & m, c_black);
        }
    }
    for (i = 0; i < 16u; i++) {
        int bt = map_fnpad[i];
        if (bt >= 0) {
            uint32_t m = 1u << bt;
            led_level((i / 4u) * 20u + 16u + (i % 4u), lt_btn & m, lt_midb & m, lt_dimb & m, c_btn);
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
