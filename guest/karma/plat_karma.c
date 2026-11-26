/* KARMA (eatyourgreensstudio-dotcom/karma-fm1, GPL-3.0 ; groovebox 4 pistes dérivée de SLOOP, avec les modèles
 * Plaits / Elements / Clouds de Mutable Instruments, MIT) en invité PGI sur le Play+ (Avrek OS).
 * Code à nous (AvrekStudio) ; on ne touche à rien dans KARMA.
 * Comme son émulateur web (web/emu/sloop_web.c) : son harnais tests/ui_pages_test.c (UI_PAGES_HARNESS_ONLY, la vraie
 * interface et le vrai son sur des doublures), compilé ici avec des en-têtes d'hôte vides (guest/sdk/hostshim), la
 * partie C++ (firmware/mi : Plaits, Elements) compilée à part, puis guest/sdk/felucca_pgi.h. Un seul cœur.
 * Pas encore : la sauvegarde, USB, MIDI. Licence : GPL-3.0 ; le .pgi est pour un usage personnel. */
#include <stdint.h>
#include "pgi_guest.h"
static const struct pgi_host *host;
#define FM1_NCOL 11u
static const int8_t FM1_KEYMAP[6][FM1_NCOL] = {
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 5, 11,  4, 10,  3,  9,  2,  8, -1, -1, -1},
    {34, 35, 36, 37, 38, 40, 39, 13,  7,  6, 12},
    {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
    { 0,  1, 15, 14, 17, 16, 19, 18, 20, 21, 22},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
};
#define UI_PAGES_HARNESS_ONLY 1
#include "ui_pages_test.c"
#undef __attribute__

static void felucca_boot(void)                  /* web_boot de web/emu/sloop_web.c */
{
    uint32_t i;
    bank_resolve();
    panel = PANEL_DEFAULT;
    layers_init();
    settings.palette = 4;
    palette_set(4);
    host_tracks_init();
    song.g[G_BPM] = 90;
    song.g[G_ROUTE] = (int16_t)lights_min;
    song.g[G_KNOBS] = (int16_t)midi_knobs;
    for (i = 0; i < NPART; i++) {
        set_engine_of(&trk[i], TRK_DEF[i][0]);
        apply_preset_to(&trk[i], TRK_DEF[i][1]);
        trk[i].engine = trk[i].eng_req;
    }
    TDRUM->p[P_E0] = DRUM_DEFAULT_KIT;
    lcd_fill(0, 0, 240, 240, C_BLACK);
    go_home();
    ui.force = 1;
    boot_t = 0;
}
#define felucca_scope(o) do { uint32_t i_; for (i_ = 1; i_ < CTL; i_ += 2u) { \
        scope_bufr[scope_w & (SCOPE_N - 1u)] = vis_tap[2u * i_ + 1u]; \
        scope_buf[scope_w++ & (SCOPE_N - 1u)] = vis_tap[2u * i_]; } } while (0)
#define FELUCCA_GUEST_NAME "KARMA"
#include "felucca_pgi.h"
