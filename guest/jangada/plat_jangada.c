/* Jangada (zednaked/jangada, GPL-3.0 ; firmware FM-1 de la famille Felucca : superwave analogique, matrice de
 * modulation…) en invité PGI sur le Play+ (Avrek OS). Code à nous (AvrekStudio) ; on ne touche à rien dans Jangada.
 * Jangada n'a ni émulateur web ni couche plateforme : on inclut ses sources dans l'ordre de son test de l'interface
 * (tests/layers_test.c : la bibliothèque, l'écran, le son, le séquenceur, l'interface, les projets), avec nos
 * doublures du panneau, puis la moitié commune guest/sdk/felucca_pgi.h (son par mix_block sous l'interruption audio,
 * interface dans la boucle, écran, matrice, LED). La mise sous tension : felucca_init de son main.c.
 * Pas encore : la sauvegarde (rien n'est écrit en flash), USB, MIDI. Un seul cœur.
 * Licence : GPL-3.0 ; le .pgi est pour un usage personnel, ne pas le distribuer. */
#include <stdint.h>
#include "pgi_guest.h"
static const struct pgi_host *host;
#define __attribute__(x)
#include "felucca_tables.h"
static void felucca_init(void);                 /* (ui_menu.c : NEW PROJECT ; plus bas, celle de main.c) */
#include "libc.c"
static struct { volatile uint32_t notes, buttons; } fm1_in;
static volatile uint32_t fm1_ms;
static uint16_t screen[240 * 240];
static void lcd_sync(void) {}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    for (j = y; j < y + h && j < 240u; j++)
        for (i = x; i < x + w && i < 240u; i++)
            screen[j * 240u + i] = (uint16_t)((c >> 8) | (c << 8));
}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    uint32_t i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (y + j < 240u && x + i < 240u)
                screen[(y + j) * 240u + x + i] = p[j * w + i];
}
static void fm1_delay_ms(uint32_t ms) { (void)ms; }
static uint32_t fm1_ticks(void) { return fm1_ms * 1000u; }
static void fm1_wdt_feed(void) {}
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}
static void fm1_led_key(uint32_t k, int on) { (void)k; (void)on; }
static int32_t encs[7];
static int32_t fm1_enc_take(uint32_t i)
{
    int32_t v = encs[i % 7u];
    encs[i % 7u] = 0;
    return v;
}
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
static int32_t fm1_adc_read(int c) { (void)c; return -1; }
#include "gfx.c"
#include "core.h"
#include "engines.c"
#include "mod.c"
#include "drums.c"
#include "params.c"
#include "voice.c"
#include "slicer.c"
#include "fx.c"
#include "usb.c"
#include "midi_uart.c"
#include "seq.c"
struct felucca_dbg { uint32_t in_audio, late, halves, last_us, stage; } felucca_dbg;
#define SCOPE_N 512u
static int16_t scope_buf[SCOPE_N];
static uint32_t scope_w;
#include "panel.c"
#include "ui.c"
#include "icons.c"
#include "ui_draw.c"
#include "ui_menu.c"
#define FM1_NCOL 11
#define FM1_TICKS_PER_US 1
static uint8_t fm1_led[FM1_NCOL], fm1_led_dim[FM1_NCOL], fm1_led_bg[FM1_NCOL], fm1_led_breath[FM1_NCOL];
static uint16_t fm1_led_bg_ns;
static const int8_t FM1_KEYMAP[6][FM1_NCOL] = {
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 5, 11,  4, 10,  3,  9,  2,  8, -1, -1, -1},
    {34, 35, 36, 37, 38, 40, 39, 13,  7,  6, 12},
    {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
    { 0,  1, 15, 14, 17, 16, 19, 18, 20, 21, 22},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
};
#include "ui_input.c"
#include "upreset.c"
#include "project.c"
#undef __attribute__

static void felucca_init(void)                  /* celle de main.c */
{
    uint32_t i;
    fm6_init();
    for (i = 0; i < G_SETTINGS; i++)
        song.g[i] = GP[i].def;
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        track_defaults(t);
        if (i < NPART) {
            set_engine_raw(t, TRK_DEF[i][0]);
            apply_preset_to(t, TRK_DEF[i][1]);
            t->engine = t->eng_req;
        }
        track_defaults_steps(t);
    }
    song.sel = 0;
    song.solo = 0;
    song.rec = 0;
    song.master_q12 = 2048;
    ui.home = 1;
    ui.force = 1;
}
static void felucca_boot(void)
{
    panel_init();
    settings_init();                            /* (fm1_main de main.c, avant l'écran) : le thème. Sans lui, les
                                                 * couleurs de l'interface (ux.bg, pal[]...) restaient à 0 : tout
                                                 * noir sur noir (vu sur la machine le 27/11). Palette par défaut
                                                 * CHOQUE, pas de flash relue (persist_boot) */
    felucca_init();
    lcd_fill(0, 0, 240, 240, 0);
}
#define felucca_scope(o) do { uint32_t i_; for (i_ = 1; i_ < CTL; i_ += 2u) \
        scope_buf[scope_w++ & (SCOPE_N - 1u)] = (int16_t)(o)[2u * i_]; } while (0)   /* (comme son audio.c) */
#define FELUCCA_GUEST_NAME "JANGADA"
#include "felucca_pgi.h"
