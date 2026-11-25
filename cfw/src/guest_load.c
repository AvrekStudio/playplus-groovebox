/* Mode invité : chargement d'un invité PGI depuis la carte SD (jalon M4). Format : voir pgi.h.
 *
 * guest_load(chemin) (boucle principale : action du menu ou sélecteur) :
 *   0. lit l'en-tête seul : ABI 1 -> OCRAM ; ABI 2 -> son champ base (OCRAM ou SDRAM). Pour la SDRAM
 *      (haut de la mémoire des samples de Polyend, voir pgi.h), refus si le curseur des samples y arrive ;
 *   1. lit le fichier par les fonctions fichier de l'officiel (fw_file_*, enveloppe FatFs, objet de 0x70 o,
 *      FA_READ = 1 ; mêmes appels que les outils FreePlay) directement à PGI_BASE, par blocs de 4 Ko ;
 *   2. vérifie l'en-tête : magie, ABI, image_end = fin des octets lus, bss_end dans la zone, et chaque
 *      point d'entrée dans l'image (adresse Thumb impaire) — un fichier abîmé ne doit pas planter ;
 *   3. met .bss à zéro ;
 *   4. déclare la zone à la protection mémoire (MPU) : région 15 (la plus prioritaire), la zone de
 *      l'invité (OCRAM 512 Ko à 0x20200000, ou SDRAM 2 Mo à 0x81E00000), mémoire normale en cache écriture
 *      différée, accès complet, EXÉCUTABLE. Le démarrage
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
extern int fw_file_write(void *f, const void *buf, u32 n);
extern int fw_f_mkdir(const char *path);
extern volatile u32 fw_millis;
extern char fw_pad_leds[];
extern void fw_led_encode(void *led, u32 i, u32 r, u32 g, u32 b);
extern volatile u32 fw_sample_pool[3];     /* début, capacité (échantillons int16), curseur présumé */
extern void fw_eve_cs_low(void);
extern void fw_eve_cs_high(void);
extern void fw_eve_pause(void);
extern void fw_eve_wr_addr(u32 addr);
extern void fw_eve_spi_byte(u32 b);

#define FA_READ 0x01
#define FA_WRITE_NEW 0x0a        /* FA_WRITE | FA_CREATE_ALWAYS (comme les outils FreePlay) */
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
 * SIZE = n (2^(n+1) octets), ENABLE. 512 Ko : n = 18 ; 2 Mo : n = 20. */
#define RASR_NORMAL_WB 0x030B0001u
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

/* Image de l'invité -> RAM_G de l'EVE à GUEST_FB_RAMG, rangées [y0, y1), en RGB565 petit-boutiste.
 * Écriture directe en mémoire (transaction SPI : CS bas, adresse, octets, CS haut : les mêmes fonctions que
 * l'officiel pour ses rafales), pas par l'anneau de commandes. guest_screen.c l'affiche ensuite. */
static void host_screen(const unsigned short *px, unsigned int y0, unsigned int y1, unsigned int swap)
{
    u32 i, n;
    if (!guest_drawing() || y0 >= y1 || y1 > GUEST_FB_H)
        return;
    fw_eve_cs_low();
    fw_eve_pause();
    fw_eve_wr_addr(GUEST_FB_RAMG + y0 * GUEST_FB_W * 2);
    n = (y1 - y0) * GUEST_FB_W;
    px += y0 * GUEST_FB_W;
    for (i = 0; i < n; i++) {
        u32 c = px[i];
        if (swap)
            c = ((c >> 8) & 0xff) | ((c & 0xff) << 8);
        fw_eve_spi_byte(c & 0xff);
        fw_eve_spi_byte((c >> 8) & 0xff);
    }
    fw_eve_cs_high();
    fw_eve_pause();
    guest_state()->bitmap = 1;
}

/* « /Guests/<nom de l'invité>/<name> » ; 0 si un nom ne convient pas. dir_only : sans « /<name> ». */
static int ok_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-' ||
           c == '_';
}

static int guest_path(char *out, const char *name, int dir_only)
{
    const struct pgi_header *h = (const struct pgi_header *)guest_state()->pgi_base;
    const char *d = PGI_DIR;
    u32 k = 0, i;
    if (!h)
        return 0;
    while (*d)
        out[k++] = *d++;
    out[k++] = '/';
    for (i = 0; i < 15 && h->name[i]; i++) {
        if (!ok_char(h->name[i]))
            return 0;
        out[k++] = h->name[i];
    }
    if (!i)
        return 0;
    if (!dir_only) {
        out[k++] = '/';
        for (i = 0; name[i]; i++) {
            if (i >= 24 || !ok_char(name[i]) || (i == 0 && name[0] == '.'))
                return 0;
            out[k++] = name[i];
        }
        if (!i)
            return 0;
    }
    out[k] = 0;
    return 1;
}

static int host_file_read(const char *name, void *dst, unsigned int max)
{
    u8 f[FILE_OBJ] __attribute__((aligned(4)));
    char path[64];
    u32 i, total = 0;
    int n;
    if (!guest_path(path, name, 0))
        return -1;
    for (i = 0; i < FILE_OBJ; i++)
        ((volatile u8 *)f)[i] = 0;
    if (!fw_file_open(f, path, FA_READ))
        return -1;
    while (total < max) {
        n = fw_file_read(f, (u8 *)dst + total, max - total < CHUNK ? max - total : CHUNK);
        if (n <= 0)
            break;
        total += (u32)n;
    }
    fw_file_close(f);
    return (int)total;
}

static int host_file_write(const char *name, const void *src, unsigned int len)
{
    u8 f[FILE_OBJ] __attribute__((aligned(4)));
    char path[64];
    u32 i;
    int n;
    if (!guest_path(path, name, 1))
        return -1;
    fw_f_mkdir(PGI_DIR);                 /* 0 créé, 8 existe déjà : dans les deux cas on continue */
    fw_f_mkdir(path);
    if (!guest_path(path, name, 0))
        return -1;
    for (i = 0; i < FILE_OBJ; i++)
        ((volatile u8 *)f)[i] = 0;
    if (!fw_file_open(f, path, FA_WRITE_NEW))
        return -1;
    n = len ? fw_file_write(f, src, len) : 0;
    fw_file_close(f);
    return n == (int)len ? 0 : -1;
}

static const struct pgi_host host = {PGI_ABI, host_millis, host_led, host_screen, host_file_read, host_file_write};

/* ---- chargement ---------------------------------------------------------------------------- */

static int entry_ok(const void *fn, u32 base, u32 end)
{
    u32 a = (u32)fn;
    return a == 0 || ((a & 1) && a > base && a < end);   /* 0 = point d'entrée absent */
}

void guest_mem_prepare(u32 base, u32 sizebits, u32 end)
{
    u32 a;
    MPU_RNR = PGI_REGION;
    MPU_RBAR = base;
    MPU_RASR = RASR_NORMAL_WB | ((sizebits - 1) << 1);
    MPU_CTRL = MPU_CTRL | 1u;           /* déjà active (CTRL = 5 dans l'officiel) : on ne fait que vérifier */
    dsb();
    isb();
    for (a = base & ~31u; a < end; a += 32)
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
    guest_state()->bitmap = 0;
    dsb();
}

/* 1 si un invité valide est chargé et initialisé, 0 sinon (pas de fichier, fichier refusé). */
int guest_load(const char *path)
{
    u8 f[FILE_OBJ] __attribute__((aligned(4)));
    u32 hd[sizeof(struct pgi_header) / 4];
    const struct pgi_header *h0 = (const struct pgi_header *)hd, *h;
    u32 base, limit, sizebits, total = 0, i;
    int n;

    guest_unload();
    for (i = 0; i < FILE_OBJ; i++)
        ((volatile u8 *)f)[i] = 0;
    if (!fw_file_open(f, path, FA_READ))
        return 0;
    n = fw_file_read(f, hd, sizeof hd);
    fw_file_close(f);
    if (n != (int)sizeof hd || h0->magic != PGI_MAGIC || h0->abi < 1 || h0->abi > PGI_ABI)
        return 0;
    base = h0->abi >= 2 ? h0->base : PGI_BASE;
    if (base == PGI_BASE) {
        limit = PGI_LIMIT;
        sizebits = 19;
    } else if (base == PGI_SDRAM_BASE) {
        u32 pool_end = fw_sample_pool[0] + 2 * fw_sample_pool[1];
        if (fw_sample_pool[2] > PGI_SDRAM_BASE || pool_end < PGI_SDRAM_BASE + PGI_SDRAM_LIMIT)
            return 0;                   /* les samples du projet occupent la zone, ou carte mémoire inattendue */
        limit = PGI_SDRAM_LIMIT;
        sizebits = 21;
    } else {
        return 0;
    }

    for (i = 0; i < FILE_OBJ; i++)
        ((volatile u8 *)f)[i] = 0;
    if (!fw_file_open(f, path, FA_READ))
        return 0;
    while (total < limit) {
        u32 want = limit - total < CHUNK ? limit - total : CHUNK;
        n = fw_file_read(f, (u8 *)base + total, want);
        if (n <= 0)
            break;
        total += (u32)n;
    }
    fw_file_close(f);

    h = (const struct pgi_header *)base;
    if (total < sizeof(struct pgi_header) || h->magic != PGI_MAGIC || h->abi != h0->abi ||
        h->image_end != base + total || h->bss_end < h->image_end || h->bss_end > base + limit ||
        !entry_ok((const void *)h->init, base, h->image_end) || !entry_ok((const void *)h->audio, base, h->image_end) ||
        !entry_ok((const void *)h->event, base, h->image_end) || !entry_ok((const void *)h->tick, base, h->image_end))
        return 0;
    for (i = h->image_end; i < h->bss_end; i += 4)
        *(volatile u32 *)i = 0;
    guest_mem_prepare(base, sizebits, h->bss_end);
    guest_state()->pgi_base = base;    /* avant init : init peut déjà lire ses fichiers (guest_path) */
    if (h->init)
        h->init(&host);
    guest_state()->pgi = 1;            /* après init seulement : l'interruption audio appelle alors l'invité */
    return 1;
}

const struct pgi_header *guest_pgi(void)
{
    return guest_state()->pgi ? (const struct pgi_header *)guest_state()->pgi_base : 0;
}
