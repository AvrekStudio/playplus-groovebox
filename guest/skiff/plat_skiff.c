/* Skiff (illuminus5/skiff, GPL-3.0 ; groovebox 6 pistes de la famille Felucca) en invité PGI sur le Play+ (Avrek OS).
 * Code à nous (AvrekStudio) ; on ne touche à rien dans Skiff.
 * Son émulateur web (web/emu/felucca_web.c : tout le FM-1 simulé, horloge, interruption audio, boucle, flash en RAM)
 * compilé ici avec des en-têtes d'hôte vides (guest/sdk/hostshim), puis guest/sdk/webapi_pgi.h : la boucle du Play+
 * le fait avancer pour garder une réserve de son, l'interruption audio ne fait que la lire. La flash simulée est
 * gardée sur la carte (/Guests/SKIFF/flash.bin) : projets, sons et réglages restent. Un seul cœur.
 * Licence : GPL-3.0 ; le .pgi est pour un usage personnel, ne pas le distribuer. */
#include <stdint.h>
#include "pgi_guest.h"
static const struct pgi_host *host;
#include "felucca_web.c"
#undef __attribute__
#define WA_NAME "SKIFF"
#define WA_BOOT() web_boot()
#define WA_RENDER(n) web_render(n)
#define WA_OUT_L web_out_l()
#define WA_OUT_R web_out_r()
#define WA_SCREEN() web_screen()
#define WA_BUTTONS(m) web_buttons(m)
#define WA_KEYS(m) web_keys(m)
#define WA_ENC(r, n) web_enc(r, n)
#define WA_MASTER(v) web_master(v)
#define WA_LIT_BUTTONS() web_lit_buttons()
#define WA_LIT_KEYS() web_lit_keys()
#define WA_DIM_BUTTONS() (web_dim_buttons() | web_breath_buttons())
#define WA_DIM_KEYS() (web_dim_keys() | web_breath_keys() | web_mid_keys())
#define WA_NOR() web_nor()
#define WA_NOR_SIZE() web_nor_size()
#define WA_NOR_ERASE() web_nor_erase()
#define WA_FLASH_WRITES() web_flash_writes_count()
#define WA_PLAYING() web_playing()
#include "webapi_pgi.h"
