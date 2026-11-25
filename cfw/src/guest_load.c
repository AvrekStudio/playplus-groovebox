/* Mode invité : chargement d'un invité PGI depuis la carte SD (jalon M4). Format : voir pgi.h.
 *
 * guest_load(chemin) (boucle principale : action du menu ou sélecteur) :
 *   1. lit le fichier par les fonctions fichier de l'officiel (fw_file_*, enveloppe FatFs, objet de 0x70 o,
 *      FA_READ = 1 ; mêmes appels que les outils FreePlay) directement à PGI_BASE, par blocs de 4 Ko ;
 *   2. vérifie l'en-tête : magie, ABI, image_end = fin des octets lus, bss_end dans la zone, et chaque
 *      point d'entrée dans l'image (adresse Thumb impaire) — un fichier abîmé ne doit pas planter ;
 *   3. met .bss à zéro ;
 *   4. déclare la zone à la protection mémoire (MPU) : région 15 (la plus prioritaire), 512 Ko à
 *      0x20200000, mémoire normale en cache écriture différée, accès complet, EXÉCUTABLE. Le démarrage
 *      de l'officiel (0x600b55a8, décodé statiquement le 25/11/2026) semble déjà l'autoriser à
 *      l'exécution, sans cache ; on ne dépend pas de ce décodage. Les régions 0 à 10 sont prises par
 *      l'officiel (sauf 9), la 15 est libre ;
 *   5. nettoie le cache de données sur la zone (pour que le code écrit soit en mémoire) et invalide le
 *      cache d'instructions, puis appelle init de l'invité.
 * Tant que le chargement n'est pas fini, guest_state()->pgi vaut 0 : l'interruption audio ne touche pas
 * à l'invité. Le Play+ virtuel n'émule ni la MPU ni les caches : les étapes 4 et 5 ne sont vérifiables
 * que sur la machine. */
#include "cfw.h"
#include "target.h"
#include "guest.h"
#include "pgi.h"

extern int fw_file_open(void *f, const char *path, u32 mode);
extern int fw_file_read(void *f, void *buf, u32 n);
extern void fw_file_close(void *f);
extern volatile u32 fw_millis;
extern char fw_pad_leds[];
extern void fw_led_encode(void *led, u32 i, u32 r, u32 g, u32 b);

#define FA_READ 0x01
#define FILE_OBJ 0x70
#define CHUNK 4096

/* registres du Cortex-M7 */
#define MPU_CTRL (*(volatile u32 *)0xE000ED94)
#define MPU_RNR (*(volatile u32 *)0xE000ED98)
#define MPU_RBAR (*(volatile u32 *)0xE000ED9C)
#define MPU_RASR (*(volatile u32 *)0xE000EDA0)
#define SCB_ICIALLU (*(volatile u32 *)0xE000EF50)
#define SCB_DCCMVAC (*(volatile u32 *)0xE000EF68)
/* RASR : XN=0, AP=011 (lecture/écriture), TEX=001 C=1 B=1 (normale, écriture différée + allocation),
 * SIZE = 18 (2^(18+1) = 512 Ko), ENABLE */
#define PGI_RASR 0x030B0025u
#define PGI_REGION 15

static inline void dsb(void) { __asm__ volatile("dsb" ::: "memory"); }
static inline void isb(void) { __asm__ volatile("isb" ::: "memory"); }

/* ---- services offerts à l'invité (struct pgi_host) ------------------------------------------- */

static unsigned int host_millis(void)
{
    return fw_millis;
}

static void host_led(unsigned int i, unsigned int r, unsigned int g, unsigned int b)
{
    if (i < 160 && guest_drawing())
        fw_led_encode(fw_pad_leds, i, r & 0xff, g & 0xff, b & 0xff);
}

static const struct pgi_host host = {PGI_ABI, host_millis, host_led};

/* ---- chargement ---------------------------------------------------------------------------- */

static int entry_ok(const void *fn, u32 end)
{
    u32 a = (u32)fn;
    return a == 0 || ((a & 1) && a > PGI_BASE && a < end);   /* 0 = point d'entrée absent */
}

static void prepare_memory(u32 end)
{
    u32 a;
    MPU_RNR = PGI_REGION;
    MPU_RBAR = PGI_BASE;
    MPU_RASR = PGI_RASR;
    MPU_CTRL = MPU_CTRL | 1u;           /* déjà active (CTRL = 5 dans l'officiel) : on ne fait que vérifier */
    dsb();
    isb();
    for (a = PGI_BASE & ~31u; a < end; a += 32)
        SCB_DCCMVAC = a;
    dsb();
    SCB_ICIALLU = 0;
    dsb();
    isb();
}

/* L'invité ne reçoit plus rien : l'interruption audio et la boucle cessent de l'appeler. */
void guest_unload(void)
{
    guest_state()->pgi = 0;
    dsb();
}

/* 1 si un invité valide est chargé et initialisé, 0 sinon (pas de fichier, fichier refusé). */
int guest_load(const char *path)
{
    u8 f[FILE_OBJ] __attribute__((aligned(4)));
    u8 *dst = (u8 *)PGI_BASE;
    const struct pgi_header *h = (const struct pgi_header *)PGI_BASE;
    u32 total = 0, i;
    int n;

    guest_state()->pgi = 0;
    for (i = 0; i < FILE_OBJ; i++)
        ((volatile u8 *)f)[i] = 0;
    if (!fw_file_open(f, path, FA_READ))
        return 0;
    while (total < PGI_LIMIT) {
        u32 want = PGI_LIMIT - total < CHUNK ? PGI_LIMIT - total : CHUNK;
        n = fw_file_read(f, dst + total, want);
        if (n <= 0)
            break;
        total += (u32)n;
    }
    fw_file_close(f);

    if (total < sizeof(struct pgi_header) || h->magic != PGI_MAGIC || h->abi != PGI_ABI ||
        h->image_end != PGI_BASE + total || h->bss_end < h->image_end || h->bss_end > PGI_BASE + PGI_LIMIT ||
        !entry_ok((const void *)h->init, h->image_end) || !entry_ok((const void *)h->audio, h->image_end) ||
        !entry_ok((const void *)h->event, h->image_end) || !entry_ok((const void *)h->tick, h->image_end))
        return 0;
    for (i = h->image_end; i < h->bss_end; i += 4)
        *(volatile u32 *)i = 0;
    prepare_memory(h->bss_end);
    if (h->init)
        h->init(&host);
    guest_state()->pgi = 1;
    return 1;
}

const struct pgi_header *guest_pgi(void)
{
    return guest_state()->pgi ? (const struct pgi_header *)PGI_BASE : 0;
}
