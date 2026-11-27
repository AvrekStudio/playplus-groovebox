/* CoralFlower (FicusFicusov/CoralFlower, GPL-3.0 ; instrument d'accords du FM-1, d'après ChoralRoot FM-1 et Felucca)
 * en invité PGI sur le Play+ (Avrek OS). Code à nous (AvrekStudio) ; on ne touche à rien dans CoralFlower.
 * Son émulateur web (tools/emu/web/emu_web.c : le firmware sur la HAL de l'émulateur, temps simulé, flash de 1 Mo en
 * RAM) compilé ici avec des en-têtes d'hôte vides (guest/sdk/hostshim), puis guest/sdk/webapi_pgi.h (en mode son sous
 * interruption depuis le 27/11 : voir cf_loop / cf_isr). La flash simulée
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
/* (27/11) Son sous l'interruption audio du Play+, interface dans la boucle, comme sur le FM-1 : sur la machine, le
 * son rendu dans la boucle (web_render : tout l'appareil, interface comprise, avance par morceaux) grésillait à
 * l'attaque des notes (CALC jusqu'à 77 % : un dessin lourd vidait la réserve de 35 ms). Les sections critiques de
 * CoralFlower (fm1_irq_off / fm1_irq_on de son émulateur) masquent maintenant les interruptions : shim/os/lock.h.
 *   cf_loop : le temps du firmware suit l'horloge du Play+ (emu_fw_tick par milliseconde : touches, boutons,
 *             encodeurs), puis une image toutes les 15 ms (emu_fw_frame) ou la lecture des touches entre deux
 *             (emu_fw_idle), comme run_ms d'emu_web.c, sans le son ;
 *   cf_isr  : emu_fw_audio, l'ISR ALNK0 du FM-1 (un demi-tampon de 128 trames, HALF_FRAMES), désentrelacé. */
static uint32_t cf_t0, cf_started;
static void cf_loop(uint32_t now)
{
    uint32_t target, k = 0;
    if (!booted)
        return;
    if (!cf_started) {
        cf_started = 1;
        cf_t0 = now - dev_ms;
    }
    target = now - cf_t0;
    if (target - dev_ms > 200u && target - dev_ms < 0x80000000u) {
        cf_t0 += target - dev_ms - 20u;         /* (un long arrêt, une écriture sur la carte : on ne rattrape pas tout) */
        target = now - cf_t0;
    }
    while ((int32_t)(target - dev_ms) > 0 && k++ < 20u)
        emu_fw_tick(dev_ms++);
    if (dev_ms - last_frame >= 15u + emu_render_ms) {
        last_frame = dev_ms;
        emu_fw_frame();
    } else
        emu_fw_idle();
}
static int16_t cf_blk[2 * 128];
static void cf_isr(short *l, short *r, unsigned int n)
{
    unsigned int i, k, m;
    for (k = 0; k < n; k += m) {
        m = n - k > 128u ? 128u : n - k;
        emu_fw_audio(cf_blk, m);
        for (i = 0; i < m; i++)
            l[k + i] = cf_blk[2 * i], r[k + i] = cf_blk[2 * i + 1];
    }
}
#define WA_LOOP(now) cf_loop(now)
#define WA_ISR(l, r, n) cf_isr(l, r, n)
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
