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
        emu_sim = 0;                            /* fm1_ticks = le vrai temps (shim/time.h) : voir plus haut */
        /* (27/11, 3e essai) Sous l'interruption audio, l'horloge « modèle » de l'émulateur (emu_hal_fw.h, celle de
         * CF_LOADMODEL) : les cycles du Cortex-M7 comptés CF_CLOCK_X fois. CoralFlower règle ses voix pour tenir 82 %
         * d'un demi-tampon (voice.c VL_TARGET) et en coupe au-delà de 85 % (audio.c SHED_TICKS) : c'est fait pour le
         * FM-1, où le son est seul. Sur le Play+, il faut laisser du temps à l'hôte et aux autres interruptions (PIC à
         * 87 % en jeu lourd et craquements à l'attaque). Avec 1,5, il vise 82 / 1,5 = 55 % réels et coupe à 57 %.
         * L'horloge de l'interface (hors interruption) reste le vrai temps. emu_model_milli : cycles x milli / 259 400
         * = µs x CF_CLOCK_X à 600 MHz, donc milli = CF_CLOCK_X x 432,3. */
        emu_model_t0 = fm1_ticks();
        emu_model_ovh = 0;
        emu_model_milli = 649u;                 /* CF_CLOCK_X = 1,5 (27/11, 13 h 19 : 1,25 craquait encore ; M 0, I 3,5 :
                                                 * le son part jusqu à 0,6 ms en retard, venu de l hôte) */
        emu_model_instr = cf_cyc64;
    }
    target = now - cf_t0;
    if (target - dev_ms > 200u && target - dev_ms < 0x80000000u) {
        cf_t0 += target - dev_ms - 20u;         /* (un long arrêt, une écriture sur la carte : on ne rattrape pas tout) */
        target = now - cf_t0;
    }
    while ((int32_t)(target - dev_ms) > 0 && k++ < 20u)
        emu_fw_tick(dev_ms++);
    /* (27/11, 2e essai) La boucle de main.c du FM-1, telle quelle : ui_input / ui_leds / ui_draw de cf_shim.c, SANS le
     * verrou qu'emu_fw_frame prend autour de toute l'image (un choix de l'émulateur : sur le FM-1, seuls de courts
     * passages masquent les interruptions, et ils le font eux-mêmes). Avec ce verrou devenu un vrai masquage, le son
     * attendait toute l'image de l'interface, plus longue qu'un demi-tampon à l'attaque d'une note : craquements. */
    if (dev_ms - last_frame >= 15u) {
        last_frame = dev_ms;
        master_poll();
        ui_input();
        settings_poll();
        ui_leds();
        ui_draw();
        cr_update_poll();
        if (fm1_ms > BG_SETTLE_MS && bootguard.pending)
            bootguard_settled(&bootguard);
    } else
        ui_input();                             /* (main.c : les touches entre deux images) */
}
static int16_t cf_blk[2 * 128];
/* diagnostic (27/11, craquements à l'attaque) : le plus grand écart entre deux appels audio (normalement 2,9 ms ; plus,
 * l'appel est parti en retard), le plus long masquage de la boucle (shim/os/lock.h), les voix délestées. Une fois par
 * seconde, dans la bande de droite à la place de MAP / SD : « M1.2 » (masquage, ms) et « I3.1 S2 » (écart, ms ;
 * délestages). */
static volatile uint32_t cf_isr_last, cf_isr_gap;
static uint32_t cf_shed0;
static char cf_d1[8], cf_d2[12];
static char *cf_ms1(char *o, uint32_t cyc)      /* cycles -> « 1.2 » (ms, une décimale) */
{
    uint32_t t = cyc / 60000u;                  /* dixièmes de ms à 600 MHz */
    if (t > 999u)
        t = 999u;
    if (t >= 100u)
        *o++ = (char)('0' + t / 100u);
    *o++ = (char)('0' + t / 10u % 10u);
    *o++ = '.';
    *o++ = (char)('0' + t % 10u);
    *o = 0;
    return o;
}
static void cf_diag(void)
{
    char *o;
    uint32_t sh = shed_count - cf_shed0;
    cf_shed0 = shed_count;
    cf_d1[0] = 'M';
    cf_ms1(cf_d1 + 1, cf_mask_max);
    cf_mask_max = 0;
    cf_d2[0] = 'I';
    o = cf_ms1(cf_d2 + 1, cf_isr_gap);
    cf_isr_gap = 0;
    *o++ = ' ', *o++ = 'S';
    *o++ = (char)('0' + (sh > 9u ? 9u : sh));
    *o = 0;
}
#define WA_DIAG() cf_diag()
#define WA_DIAG_A cf_d1
#define WA_DIAG_B cf_d2
static void cf_isr(short *l, short *r, unsigned int n)
{
    unsigned int i, k, m;
    {
        uint32_t now = *(volatile uint32_t *)0xE0001004u, d = now - cf_isr_last;
        if (cf_isr_last && d > cf_isr_gap)
            cf_isr_gap = d;
        cf_isr_last = now;
    }
    for (k = 0; k < n; k += m) {
        m = n - k > 128u ? 128u : n - k;
        emu_stall_blocks = 0;                   /* (27/11) l'émulateur imite l'effacement d'un secteur du FM-1 : 45 ms
                                                 * de son coupé (emu_hal_fw.h). Ici rien ne bloque : pas de trou */
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
