/* Auto-test (Menu > Auto-test) : ce que le Play+ virtuel ne sait pas vérifier, à lancer à la réception de
 * la machine. Les résultats s'affichent en haut de l'explorateur (guest_screen.c).
 *
 *   0 « MPU »     : d'après la protection mémoire programmée par l'officiel (régions 0..14 ; la 15 est la
 *                   nôtre), l'OCRAM (0x20200000) et la SDRAM des invités (0x81E00000) sont-elles exécutables ?
 *                   (lecture seule : sans risque)
 *   1 « Samples » : la mémoire des samples est-elle là où on l'a trouvée sur le virtuel (fin du bloc =
 *                   haut de la zone des invités, curseur dans le bloc) ? (lecture seule)
 *   2 « OCRAM »   : on écrit deux instructions (movs r0, #42 ; bx lr) en OCRAM, on prépare la zone comme
 *                   pour un invité (MPU région 15, caches), on les appelle : 42 attendu.
 *   3 « SDRAM »   : idem à 0x81E00000, seulement si la mémoire des samples n'y arrive pas.
 * Les tests 2 et 3 peuvent planter la machine si l'exécution y est interdite : avant chacun, on note son
 * numéro dans test_step (cfw_ram, en DTCM, n'est pas effacée au redémarrage : hors des sections que le
 * démarrage initialise). Au lancement suivant, un numéro resté là = ce test a planté : il est marqué
 * « PLANTE » et n'est pas relancé. */
#include "cfw.h"
#include "target.h"
#include "guest.h"
#include "pgi.h"

extern volatile u32 fw_sample_pool[3];

#define MPU_RNR (*(volatile u32 *)0xE000ED98)
#define MPU_RBAR (*(volatile u32 *)0xE000ED9C)
#define MPU_RASR (*(volatile u32 *)0xE000EDA0)
#define TEST_MARK 0x54535400u          /* "TST" + numéro du test en cours */
#define SDRAM_POOL_END 0x81ffee80u      /* fin du bloc des samples sur le virtuel (allocateur au démarrage) */

enum { TEST_NONE, TEST_OK, TEST_FAIL, TEST_CRASH };

static void set(volatile struct guest_state *g, u32 i, u32 v)
{
    g->test_res = (g->test_res & ~(3u << (2 * i))) | (v << (2 * i));
}

static u32 get(u32 i)
{
    return (guest_state()->test_res >> (2 * i)) & 3u;
}

/* bit XN de la région active la plus haute (hors 15) qui couvre addr ; 2 si aucune ne la couvre */
static u32 xn_at(u32 addr)
{
    int r;
    for (r = 14; r >= 0; r--) {
        u32 rbar, rasr, size, base;
        MPU_RNR = (u32)r;
        rbar = MPU_RBAR;
        rasr = MPU_RASR;
        if (!(rasr & 1))
            continue;
        size = ((rasr >> 1) & 31) + 1;               /* 2^size octets */
        base = rbar & ~31u;
        if (size >= 32 || (addr - base) < (1u << size))
            return (rasr >> 28) & 1;
    }
    return 2;
}

static int sdram_free(void)
{
    u32 end = fw_sample_pool[0] + 2 * fw_sample_pool[1];
    return fw_sample_pool[2] <= PGI_SDRAM_BASE && end >= PGI_SDRAM_BASE + PGI_SDRAM_LIMIT;
}

static u32 exec_at(volatile struct guest_state *g, u32 n, u32 base, u32 sizebits)
{
    volatile u16 *code = (volatile u16 *)base;
    int (*fn)(void) = (int (*)(void))(base | 1);
    int r;
    g->test_step = TEST_MARK | n;
    code[0] = 0x202a;                                /* movs r0, #42 */
    code[1] = 0x4770;                                /* bx lr */
    guest_mem_prepare(base, sizebits, base + 32);
    r = fn();
    g->test_step = 0;
    return r == 42 ? TEST_OK : TEST_FAIL;
}

void guest_selftest(void)
{
    volatile struct guest_state *g = guest_state();
    u32 crashed = 4;
    if ((g->test_step & ~0xFFu) == TEST_MARK)        /* le dernier auto-test a planté pendant ce test */
        crashed = g->test_step & 0xFFu;
    g->test_step = 0;
    g->test_res = 0;
    guest_unload();                                  /* l'OCRAM et la SDRAM des invités vont être réécrites */

    set(g, 0, xn_at(PGI_BASE) == 0 && xn_at(PGI_SDRAM_BASE) == 0 ? TEST_OK : TEST_FAIL);
    set(g, 1, fw_sample_pool[0] >= 0x80000000u && fw_sample_pool[0] + 2 * fw_sample_pool[1] == SDRAM_POOL_END &&
                      fw_sample_pool[2] >= fw_sample_pool[0] && fw_sample_pool[2] <= SDRAM_POOL_END ? TEST_OK : TEST_FAIL);
    set(g, 2, crashed == 2 ? TEST_CRASH : exec_at(g, 2, PGI_BASE, 19));
    if (crashed == 3)
        set(g, 3, TEST_CRASH);
    else if (sdram_free())
        set(g, 3, exec_at(g, 3, PGI_SDRAM_BASE, 21));
    else
        set(g, 3, TEST_FAIL);                        /* zone occupée par des samples : non testée */
    guest_enter(1);
}

/* résumé de l'auto-test en deux lignes pour l'écran : part 0 « MPU ok  Samples ok », part 1
 * « OCRAM ok  SDRAM ok ». Renvoie 0 si l'auto-test n'a pas tourné. */
u32 guest_test_line(char *out, u32 max, u32 part)
{
    static const char *const name[4] = {"MPU ", "Samples ", "OCRAM ", "SDRAM "};
    static const char *const res[4] = {"-", "ok", "ECHEC", "PLANTE"};
    u32 i, k = 0, any = 0;
    const char *s;
    for (i = 0; i < 4; i++)
        any |= get(i);
    for (i = 2 * part; i < 2 * part + 2; i++) {
        for (s = name[i]; *s && k + 1 < max; s++)
            out[k++] = *s;
        for (s = res[get(i)]; *s && k + 1 < max; s++)
            out[k++] = *s;
        if (i == 2 * part && k + 3 < max) {
            out[k++] = ' ';
            out[k++] = ' ';
        }
    }
    out[k] = 0;
    return any;
}
