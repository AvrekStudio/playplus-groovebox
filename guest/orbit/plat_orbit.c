/* ORBIT (dspaudio/orbit, GPL-3.0, groovebox de la famille SLOOP / Felucca) en invité PGI sur le Play+ (Avrek OS).
 * Code à nous (AvrekStudio) ; on ne touche à rien dans ORBIT.
 * Comme son aperçu natif (tools/orbit_host.c) : son harnais tests/orbit_host_support.h (la vraie interface, le
 * vrai son, le séquenceur, sur une image 240 x 240 et des doublures pour le panneau et la mémoire), compilé ici avec
 * des en-têtes d'hôte vides (guest/sdk/hostshim : ce que les tests écrivent dans des fichiers ne sert pas), puis la
 * moitié commune guest/sdk/felucca_pgi.h (son, boucle, écran, matrice, LED). Un seul cœur.
 * Pas encore : la sauvegarde (les doublures du harnais ne gardent rien), USB, MIDI, Bluetooth.
 * Licence : GPL-3.0 ; le .pgi est pour un usage personnel, ne pas le distribuer. */
#include <stdint.h>
#include "pgi_guest.h"
static const struct pgi_host *host;
#include "orbit_host_support.h"
#undef __attribute__

static void felucca_boot(void)                  /* la mise sous tension de tools/orbit_host.c, sans sa démo */
{
    uint32_t i;
    panel = PANEL_DEFAULT;
    layers_init();
    palette_set(5);
    host_tracks_init();
    for (i = 0; i < NPART; i++) {
        set_engine_of(&trk[i], TRK_DEF[i][0]);
        apply_preset_to(&trk[i], TRK_DEF[i][1]);
        trk[i].engine = trk[i].eng_req;
    }
    TDRUM->p[P_E0] = DRUM_DEFAULT_KIT;
    lcd_fill(0, 0, 240, 240, C_BLACK);
    go_home();
    ui.force = 1;
}
#define felucca_scope(o) do { uint32_t i_; for (i_ = 1; i_ < CTL; i_ += 2u) \
        scope_buf[scope_w++ & (SCOPE_N - 1u)] = vis_tap[2u * i_]; } while (0)
#define FELUCCA_GUEST_NAME "ORBIT"
#include "felucca_pgi.h"
