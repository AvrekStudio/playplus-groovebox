/* Melodee (keremimo/melodee, GPL-3.0 ; groovebox 4 pistes de la famille Felucca : Prophet, FM6, CZ, SID, batteries)
 * en invité PGI sur le Play+ (Avrek OS). Code à nous (AvrekStudio) ; on ne touche à rien dans Melodee.
 * Son émulateur web (web/emu/melodee_web.c, sur tests/ui_test.c : les moteurs, le séquenceur et toute l'interface sur
 * des doublures) compilé ici avec des en-têtes d'hôte vides (guest/sdk/hostshim), puis guest/sdk/webapi_pgi.h : la
 * boucle du Play+ le fait avancer pour garder une réserve de son. Son émulateur ne garde rien (pas de flash) : la
 * sauvegarde n'est pas encore là. Un seul cœur. Licence : GPL-3.0 ; le .pgi est pour un usage personnel. */
#include <stdint.h>
#include "pgi_guest.h"
static const struct pgi_host *host;
#include "melodee_web.c"
#undef __attribute__

/* l'émulateur parle en identifiants de la matrice du FM-1 ; la matrice du Play+ en étiquettes (B_*, EN_*) */
static void mel_buttons(uint32_t labels)
{
    uint32_t b, m = 0;
    for (b = 0; b < NB; b++)
        if ((labels >> b) & 1u)
            m |= 1u << panel.btn[b];
    web_buttons(m);
}
static void mel_enc(uint32_t role, int32_t n)
{
    if (role < NE)
        web_encoder(panel.enc[role], n * panel.dir[role]);
}
static uint32_t mel_lit_buttons(void)
{
    uint32_t b, m = 0, ids = web_led_buttons();
    for (b = 0; b < NB; b++)
        m |= ((ids >> panel.btn[b]) & 1u) << b;
    return m;
}
#define WA_NAME "MELODEE"
#define WA_BOOT() web_boot(724u)
#define WA_RENDER(n) web_render(n)
#define WA_OUT_L web_out_l()
#define WA_OUT_R web_out_r()
#define WA_SCREEN() web_fb()
#define WA_BUTTONS(m) mel_buttons(m)
#define WA_KEYS(m) web_keys(m)
#define WA_ENC(r, n) mel_enc(r, n)
#define WA_MASTER(v) web_master(v)
#define WA_LIT_BUTTONS() mel_lit_buttons()
#define WA_LIT_KEYS() web_led_keys()
#define WA_DIM_BUTTONS() 0u
#define WA_DIM_KEYS() 0u
#include "webapi_pgi.h"
