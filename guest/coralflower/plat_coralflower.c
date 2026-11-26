/* CoralFlower (FicusFicusov/CoralFlower, GPL-3.0 ; instrument d'accords du FM-1, d'après ChoralRoot FM-1 et Felucca)
 * en invité PGI sur le Play+ (Avrek OS). Code à nous (AvrekStudio) ; on ne touche à rien dans CoralFlower.
 * Son émulateur web (tools/emu/web/emu_web.c : le firmware sur la HAL de l'émulateur, temps simulé, flash de 1 Mo en
 * RAM) compilé ici avec des en-têtes d'hôte vides (guest/sdk/hostshim), puis guest/sdk/webapi_pgi.h. La flash simulée
 * est gardée sur la carte (/Guests/CORALFLOWER/flash.bin). Un seul cœur.
 * Licence : GPL-3.0 ; le .pgi est pour un usage personnel, ne pas le distribuer. */
#include <stdint.h>
#include "pgi_guest.h"
static const struct pgi_host *host;
#include "emu_web.c"
#undef __attribute__
/* les LED : web_leds() rend [0..26] touches, [27..40] boutons (étiquettes), 0 éteinte 1 faible 2 allumée 3 moyenne
 * 4 très faible */
static uint32_t cf_led_bits(int base, int n, int lit)
{
    const uint8_t *l = web_leds();
    uint32_t m = 0;
    int i;
    for (i = 0; i < n; i++)
        if (lit ? (l[base + i] == 2 || l[base + i] == 3) : (l[base + i] == 1 || l[base + i] == 4))
            m |= 1u << i;
    return m;
}
#define WA_NAME "CORALFLOWER"
#define WA_BOOT() web_boot()
#define WA_RENDER(n) web_render(n)
#define WA_OUT_L web_out_l()
#define WA_OUT_R web_out_r()
#define WA_SCREEN() web_fb()
#define WA_BUTTONS(m) web_buttons(m)
#define WA_KEYS(m) web_keys(m)
#define WA_ENC(r, n) web_enc((int)(r), n)
#define WA_MASTER(v) web_master((int32_t)(v))
#define WA_LIT_BUTTONS() cf_led_bits(EMU_NKEY, EMU_NB, 1)
#define WA_LIT_KEYS() cf_led_bits(0, EMU_NKEY, 1)
#define WA_DIM_BUTTONS() cf_led_bits(EMU_NKEY, EMU_NB, 0)
#define WA_DIM_KEYS() cf_led_bits(0, EMU_NKEY, 0)
#define WA_NOR() web_flash()
#define WA_NOR_SIZE() web_flash_size()
#define WA_NOR_STAGE() web_flash_stage()
#define WA_NOR_STAGED(n) web_flash_stage_len(n)
#define WA_FLASH_WRITES() web_flash_dirty()
#include "webapi_pgi.h"
