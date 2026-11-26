/* SLOOP (isod89/sloop-fm1, GPL-3.0, groovebox 4 pistes de la famille Felucca) en invité PGI sur le Play+
 * (Avrek OS). Code à nous (AvrekStudio) ; on ne touche à rien dans SLOOP.
 *
 * SLOOP n'a pas de couche plateforme séparée (plat.h) comme X0X / Tonic : son main.c parle au matériel du FM-1.
 * Mais ses tests font tourner la vraie interface et le vrai son sur des doublures (tests/ui_pages_test.c :
 * ui*.c, séquenceur, voix, moteurs et effets sur une image 240 x 240, avec des doublures pour le panneau, la
 * mémoire flash et le stockage ; le son, mix_block, entre deux images), et c'est ainsi que son dérivé KARMA le
 * fait tourner dans le navigateur (web/emu/sloop_web.c). On fait la même chose contre le Play+ :
 *   - les sources du firmware incluses ici dans l'ordre de tests/hostsim.c + tests/ui_pages_test.c (sans ce
 *     qui est propre aux tests : stdio, fils, fichiers) ;
 *   - son : mix_block par blocs de CTL trames, sous l'interruption audio du Play+ (comme l'ISR du FM-1) ;
 *   - boucle : ui_input / ui_leds / ui_draw toutes les 16 ms (main.c), ui_input entre deux ;
 *   - écran : l'image `screen` (RGB565, octets permutés comme le panneau) vers pgi_host.screen ;
 *   - commandes : la matrice (map_playplus.h, docs/correspondance/sloop.json, ou /Guests/SLOOP/map.json) vers
 *     fm1_in.buttons / fm1_in.notes / encs[] / MASTER, par les étiquettes du FM-1 (panel.c) ;
 *   - LED : fm1_led / fm1_led_dim / fm1_led_bg relues par la matrice de touches du FM-1 (comme sloop_web.c) ;
 *   - un seul cœur (cpu1_on = 0 : SLOOP rend tout sur le premier, comme un FM-1 dont le second cœur a lâché).
 * Pas encore : la sauvegarde (projets, sections, réglages : comme l'émulateur web, rien n'est gardé), USB,
 * MIDI. Licence : GPL-3.0 ; le .pgi est pour un usage personnel, ne pas le distribuer. */
#include <stdint.h>
#include "pgi_guest.h"

static const struct pgi_host *host;

/* ---- tests/hostsim.c, la tête : la bibliothèque du firmware, le cœur, le son, le séquenceur -------------- */
#define FELUCCA_ARRANGER 1
#define __attribute__(x)                        /* (comme hostsim.c : pas de sections de RAM du FM-1) */
#include "felucca_tables.h"
#include "libc.c"
static struct { volatile uint32_t notes, buttons; } fm1_in;
static volatile uint32_t fm1_remote_btn;
static void fm1_irq_off(void) {}                /* (le son appelle mix_block sous interruption, l'interface la lit */
static void fm1_irq_on(void) {}                 /*  hors interruption : comme sur le FM-1, à un cœur) */
#include "core.h"
#include "cpu1.h"
static int cpu1_on(void) { return 0; }          /* un seul cœur */
static uint32_t cpu1_ticks(void) { return 0; }
static int cpu1_run(cpu1_job_t fn, uint32_t arg) { (void)fn; (void)arg; return 0; }
static int cpu1_wait(void) { return 0; }
#include "engines.c"
#include "drums.c"
#include "params.c"
#include "voice.c"
#include "slicer.c"
#include "fx.c"
static void fm1_delay_ms(uint32_t ms) { (void)ms; }
#include "usb.c"
#include "midi_uart.c"
#include "arranger.c"
#include "seq.c"

/* ---- tests/ui_pages_test.c, la tête : l'écran, le panneau, l'interface et leurs doublures ---------------- */
static uint16_t screen[240 * 240];
static uint32_t scr_d0 = 240, scr_d1 = 0;       /* rangées touchées depuis le dernier envoi */
static void scr_touch(uint32_t y, uint32_t h)
{
    if (y < scr_d0)
        scr_d0 = y;
    if (y + h > scr_d1)
        scr_d1 = y + h > 240u ? 240u : y + h;
}
static void lcd_sync(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    uint32_t i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (x + i < 240u && y + j < 240u)
                screen[(y + j) * 240u + x + i] = p[j * w + i];
    scr_touch(y, h);
}
#include "gfx.c"
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    for (j = y; j < y + h && j < 240u; j++)
        for (i = x; i < x + w && i < 240u; i++)
            screen[j * 240u + i] = swap16(c);
    scr_touch(y, h);
}
static int32_t encs[7];
static uint32_t fm1_ticks(void) { return fm1_ms * 1000u * 24u; }
#define FM1_TICKS_PER_US 24u
static int32_t fm1_enc_take(uint32_t e)
{
    int32_t s = encs[e];
    encs[e] = 0;
    return s;
}
static uint8_t fm1_led[16], fm1_led_dim[16], fm1_led_bg[16];
static volatile uint16_t fm1_led_bg_ns;
#define FM1_NCOL 11u
static const int8_t FM1_KEYMAP[6][16] = {       /* hal/fm1_input.h : l'identifiant de touche en (colonne, bit) */
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 5, 11,  4, 10,  3,  9,  2,  8, -1, -1, -1},
    {34, 35, 36, 37, 38, 40, 39, 13,  7,  6, 12},
    {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
    { 0,  1, 15, 14, 17, 16, 19, 18, 20, 21, 22},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
};
static void fm1_led_key(uint32_t id, int on) { (void)id; (void)on; }
static uint32_t edges_btn, notes_seen;
static uint32_t fm1_input_edges(int x)
{
    uint32_t e = edges_btn;
    (void)x;
    edges_btn = 0;
    return e;
}
static uint32_t fm1_input_note_edges(void)
{
    uint32_t e = fm1_in.notes & ~notes_seen;
    notes_seen = fm1_in.notes;
    return e;
}
static void fm1_wdt_feed(void) {}
static int32_t fm1_adc_read(int c) { (void)c; return -1; }
static struct { uint32_t magic, stage, page, home, ui_frames; } felucca_dbg;
#define FELUCCA_ICONS 1
#define SCOPE_N 512u
static int16_t scope_buf[SCOPE_N], scope_bufr[SCOPE_N];
static uint32_t scope_w;
#include "panel.c"
#include "ui.c"
/* la mémoire : rien n'est gardé pour l'instant (comme l'émulateur web de KARMA) */
static int project_used(uint32_t i) { (void)i; return 0; }
static void project_save(uint32_t i) { (void)i; ui_message("NOT KEPT"); }
static void project_load(uint32_t i) { (void)i; }
static void project_erase(uint32_t i) { (void)i; }
static void arrangement_save(void) {}
static uint32_t arrangement_ready(void) { return 0; }
static void arrangement_apply(uint32_t s) { (void)s; }
static uint32_t section_bars(uint32_t s) { (void)s; return 1u; }
static void song_backup(void) {}
static void song_restore(void) {}
static void section_store(uint32_t s) { live_sec = (int8_t)s; }
static void section_load(uint32_t s) { live_sec = (int8_t)s; }
static int up_used(uint32_t k) { (void)k; return 0; }
static int up_load(uint32_t k) { (void)k; return 0; }
static int up_load_to(track_t *t, uint32_t k) { (void)t; (void)k; return 0; }
static int up_arp_of(uint32_t k, int16_t *a) { (void)k; (void)a; return 0; }
static uint32_t up_count(void) { return 0; }
static uint32_t up_nth(uint32_t n) { return n; }
static uint32_t up_rank(uint32_t s) { return s; }
static void up_name(uint32_t k, char *b) { (void)k; b[0] = 0; }
static void up_slot_label(char *b, uint32_t k) { fmt_int(b, (int32_t)k + 1); }
static void up_ui(uint32_t op, uint32_t k) { (void)op; (void)k; }
static void settings_save(void) {}
#include "ui_song.c"
#include "ui_studio.c"
#include "icons.c"
#include "ui_draw.c"
#include "ui_vis.c"
#include "ui_layers.c"
#include "ui_menu.c"
#include "ui_input.c"
#include "splash.c"
static int st_read(uint32_t off, void *dst, uint32_t n) { (void)off, (void)dst, (void)n; return -1; }
static int st_erase(uint32_t off) { (void)off; return -1; }
static int st_prog(uint32_t off, const void *src, uint32_t n) { (void)off, (void)src, (void)n; return -1; }
#include "storage.c"
#undef __attribute__

/* ---- la mise sous tension : felucca_init de main.c (sans autosave_resume : rien n'est gardé), sans logo ---- */
static void felucca_boot(void)
{
    uint32_t i;
    panel = PANEL_DEFAULT;
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    fm6_init();
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        track_defaults(t);
        if (i < NPART) {
            set_engine_of(t, TRK_DEF[i][0]);
            apply_preset_to(t, TRK_DEF[i][1]);
            t->engine = t->eng_req;
        }
        track_defaults_steps(t);
    }
    TDRUM->p[P_E0] = DRUM_DEFAULT_KIT;
    song.sel = 0;
    song.g[G_SYNC] = (int16_t)lights_sync;
    song.g[G_MIDI] = (int16_t)lights_mout;
    song.g[G_ROUTE] = (int16_t)lights_min;
    song.g[G_CH1] = midi_set[0], song.g[G_CH2] = midi_set[1], song.g[G_CH3] = midi_set[2];
    song.g[G_DRCH] = midi_set[3], song.g[G_CHOTH] = midi_set[4], song.g[G_BEND] = midi_set[5];
    song.g[G_FXCH] = midi_set[6];
    song.g[G_DRMAP] = midi_set[7];
    song.g[G_CLKOUT] = midi_set[9];
    song.g[G_RMCH] = midi_set[8];
    song.g[G_FXFADE] = fxfade;
    layers_init();
    lcd_fill(0, 0, 240, 240, C_BLACK);
    go_home();
    ui.force = 1;
}
/* l'oscilloscope du visualiseur, comme audio.c le remplit */
#define felucca_scope(o) do { uint32_t i_; for (i_ = 1; i_ < CTL; i_ += 2u) { \
        scope_bufr[scope_w & (SCOPE_N - 1u)] = vis_tap[2u * i_ + 1u]; \
        scope_buf[scope_w++ & (SCOPE_N - 1u)] = vis_tap[2u * i_]; } } while (0)

#define FP_SCR_TRACK 1
#define FELUCCA_GUEST_NAME "SLOOP"
#include "felucca_pgi.h"
