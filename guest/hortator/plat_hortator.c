/* Hortator (DeadActive/hortator, GPL-3.0 ; boîte à rythmes 8 pistes de la famille Felucca : Grids, compresseur,
 * résonateurs…) en invité PGI sur le Play+ (Avrek OS). Code à nous (AvrekStudio) ; on ne touche à rien dans Hortator.
 * Le cœur de son simulateur web (tests/sim_core.c sur tests/sim_host.h : le firmware, l'interruption audio par demi-
 * tampon, la boucle de l'interface toutes les 15 ms de son, la flash en RAM) compilé ici avec des en-têtes d'hôte
 * vides (guest/sdk/hostshim), puis guest/sdk/webapi_pgi.h. La flash simulée est gardée sur la carte
 * (/Guests/HORTATOR/flash.bin) ; la première fois, la démo du simulateur. Un seul cœur.
 * Licence : GPL-3.0 ; le .pgi est pour un usage personnel, ne pas le distribuer. */
#include <stdint.h>
#include "pgi_guest.h"
static const struct pgi_host *host;
#include "sim_core.c"
#undef __attribute__

static uint32_t ho_btn, ho_keys, ho_loaded;     /* l'API du simulateur est par appui / relâché : on lui passe les écarts */
static void ho_buttons(uint32_t m)
{
    uint32_t b, d = m ^ ho_btn;
    for (b = 0; d && b < NB; b++)
        if ((d >> b) & 1u)
            sim_btn(b, (int)((m >> b) & 1u));
    ho_btn = m;
}
static void ho_keys_set(uint32_t m)
{
    uint32_t k, d = m ^ ho_keys;
    for (k = 0; d && k < 27u; k++)
        if ((d >> k) & 1u)
            sim_key(k, (int)((m >> k) & 1u));
    ho_keys = m;
}
static uint32_t ho_writes(void)                 /* une empreinte des secteurs écrits depuis la dernière sauvegarde */
{
    const uint32_t *d = sim_flash_dirty();
    uint32_t i, s = 0;
    for (i = 0; i < SIM_FLASH_SIZE / 4096u / 32u; i++)
        s = s * 33u + d[i];
    return s;
}
#define WA_NAME "HORTATOR"
#define WA_BOOT() sim_init(!ho_loaded)
#define WA_RENDER(n) sim_render(n)
#define WA_OUT_LR sim_audio()
#define WA_SCREEN() sim_fb()
#define WA_BUTTONS(m) ho_buttons(m)
#define WA_KEYS(m) ho_keys_set(m)
#define WA_ENC(r, n) sim_enc(r, n)
#define WA_MASTER(v) sim_master(v)
#define WA_LIT_BUTTONS() sim_leds()
#define WA_LIT_KEYS() sim_key_leds()
#define WA_DIM_BUTTONS() 0u
#define WA_DIM_KEYS() 0u
#define WA_NOR() sim_flash()
#define WA_NOR_SIZE() SIM_FLASH_SIZE
#define WA_NOR_LOADED(ok) (ho_loaded = (ok))
#define WA_FLASH_WRITES() ho_writes()
#define WA_FLASH_SAVED() sim_flash_clean()
#define WA_PLAYING() sim_playing()
#include "webapi_pgi.h"
