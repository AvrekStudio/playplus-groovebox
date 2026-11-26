/* Avrek OS : l'invité (firmware FM-1 porté, ex. LOOM-8) chargé depuis la carte SD et lancé dans notre boucle.
 *
 * Repris du mode invité (guest_load.c, guest_screen.c : notre code), sans l'application Polyend autour :
 *   - CHARGEMENT (boucle principale) : OWN_GUEST_PATH, sinon PGI_PATH. En-tête lu seul d'abord (pgi.h) ;
 *     zone OCRAM ou SDRAM selon l'ABI ; pour la SDRAM (haut de la mémoire des samples de Polyend), refus si
 *     les samples du projet chargé au démarrage y arrivent ; image lue par blocs de 4 Ko ; en-tête et points
 *     d'entrée vérifiés (un fichier abîmé ne doit pas planter) ; .bss à zéro ; région MPU 15 sur la zone
 *     (mémoire normale, cache, exécutable) ; cache de données nettoyé, d'instructions invalidé ; init.
 *   - SERVICES (struct pgi_host) : millis, LED des pads, image, fichiers dans /Guests/<nom>/.
 *   - IMAGE : l'invité dessine en 240 x 240 RGB565 ; host_screen envoie les rangées modifiées dans la
 *     RAM_G de l'écran (OG_FB_RAMG). NOUVEAU : par rangées entières (480 o) avec le transfert SPI du pilote
 *     (fw_lpspi_xfer), au lieu d'un appel par octet (fw_eve_spi_byte → le même transfert pour UN octet,
 *     115 200 fois par image : c'était la lenteur de LOOM en mode invité). og_draw compose ensuite
 *     l'image : le bitmap au centre, les bandes de 40 px de l'invité (ABI 4), la charge audio.
 *   - SON : og_audio sous interruption (128 trames int16), chronométré (DWT) pour la charge affichée. */
#include "cfw.h"
#include "target.h"
#include "fmt.h"
#include "own.h"

extern int fw_file_open(void *f, const char *path, u32 mode);
extern int fw_file_read(void *f, void *buf, u32 n);
extern void fw_file_close(void *f);
extern int fw_file_write(void *f, const void *buf, u32 n);
extern int fw_f_mkdir(const char *path);
extern volatile u32 fw_millis;
extern char fw_pad_leds[];
extern void fw_led_encode(void *led, u32 i, u32 r, u32 g, u32 b);
extern volatile u32 fw_sample_pool[3];  /* début, capacité (échantillons int16), curseur présumé */
extern void fw_eve_cs_low(void);
extern void fw_eve_cs_high(void);
extern void fw_eve_pause(void);
extern void fw_eve_wr_addr(u32 addr);
extern void fw_eve_begin(void);
extern void fw_eve_end(void);
extern void fw_eve_word(u32 w);
extern void fw_eve_text(u32 x, s32 y, u32 font, u32 opts, const char *s, u32 maxlen);
extern volatile u16 fw_eve_wp;
extern int fw_lpspi_xfer(void *base, void *xfer);
extern void **volatile fw_eve_spi_dev;

#define OWN_GUEST_PATH "/Guests/LOOM-8.pgi"
#define FA_READ 0x01
#define FA_WRITE_NEW 0x0a
#define FILE_OBJ 0x70
#define CHUNK 4096
#define OG_FB_RAMG 0xE2000u             /* même place que le mode invité (guest.h) */
#define OG_W 240u
#define OG_H 240u
#define FLUSH_CYCLES 1200000u          /* 2 ms à 600 MHz d'envoi d'image par tour de boucle */
#define DWT_CYCCNT (*(volatile u32 *)0xE0001004)
#define BLOCK_BUDGET 1741497u           /* cycles d'un bloc de 128 trames à 44,1 kHz et 600 MHz */
#define CPU_WINDOW 344u                 /* ~1 s de blocs */
#define MPU_RNR (*(volatile u32 *)0xE000ED98)
#define MPU_RBAR (*(volatile u32 *)0xE000ED9C)
#define MPU_RASR (*(volatile u32 *)0xE000EDA0)
#define SCB_ICIALLU (*(volatile u32 *)0xE000EF50)
#define SCB_DCCMVAC (*(volatile u32 *)0xE000EF68)
#define RASR_NORMAL_WB 0x030B0001u      /* XN=0, AP=011, TEX=001 C=1 B=1, ENABLE (guest_load.c) */
#define PGI_REGION 15

static inline void dsb(void) { __asm__ volatile("dsb" ::: "memory"); }
static inline void isb(void) { __asm__ volatile("isb" ::: "memory"); }

static void zero(void *p, u32 n)
{
    volatile u8 *b = (volatile u8 *)p;
    while (n--)
        *b++ = 0;
}

const struct pgi_header *og_pgi(struct own *s)
{
    return s && s->pgi ? (const struct pgi_header *)s->pgi : 0;
}

/* ---- services offerts à l'invité ------------------------------------------------------------- */

static unsigned int host_millis(void)
{
    return fw_millis;
}

static void host_led(unsigned int i, unsigned int r, unsigned int g, unsigned int b)
{
    if (i < 160)
        fw_led_encode(fw_pad_leds, i, r & 0xff, g & 0xff, b & 0xff);
}

/* transfert SPI du pilote NXP : {tx, rx, taille, drapeaux} ; drapeaux comme fw_eve_spi_byte */
struct lpspi_xfer {
    const u8 *tx;
    u8 *rx;
    u32 size;
    u32 flags;
};

/* Empreinte d'une rangée (FNV-1a sur des paires de pixels) : une rangée inchangée ne repart pas. */
static u32 row_hash(const unsigned short *p)
{
    u32 h = 2166136261u, i;
    for (i = 0; i < OG_W; i += 2)
        h = (h ^ (p[i] | ((u32)p[i + 1] << 16))) * 16777619u;
    return h;
}

/* L'invité signale les rangées [y0, y1) de son image px. On ne parle PAS à l'écran ici : on note les
 * rangées qui ont vraiment changé (empreinte), og_flush les envoie depuis la boucle, par petites
 * tranches. Avant (0.2.0) les 240 rangées partaient ici d'un bloc : ~30 ms de SPI par image, et LOOM en
 * demande 60 par seconde pendant une animation : la boucle ne faisait plus que ça (pads et boutons
 * poussifs, latence ; constaté sur la machine le 26/11). */
static void host_screen(const unsigned short *px, unsigned int y0, unsigned int y1, unsigned int swap)
{
    struct own *s = own();
    u32 y;
    if (!s || !s->active || y0 >= y1 || y1 > OG_H)
        return;
    s->fb = (u32)px;
    s->fb_swap = swap;
    for (y = y0; y < y1; y++) {
        u32 h = row_hash(px + y * OG_W);
        if (h != s->row_sum[y]) {
            s->row_sum[y] = h;
            s->pend[y >> 5] |= 1u << (y & 31);
        }
    }
}

/* Boucle : envoie les rangées en attente, en suites contiguës (une adresse RAM_G par suite), pendant au
 * plus FLUSH_CYCLES, en reprenant là où on s'était arrêté (le bas de l'image n'attend pas toujours). Les
 * pixels sont lus au moment de l'envoi : si l'invité a redessiné entre-temps, c'est la dernière image
 * qui part (les images en trop sont sautées, pas mises en file). */
void og_flush(struct own *s)
{
    const unsigned short *px = (const unsigned short *)s->fb;
    u8 row[OG_W * 2] __attribute__((aligned(4)));
    struct lpspi_xfer x;
    void *base;
    u32 t0 = DWT_CYCCNT, y = s->flush_y, seen = 0, sent = 0, i;
    if (!px)
        return;
    base = *fw_eve_spi_dev;             /* premier mot de l'objet SPI : la base du LPSPI3 */
    x.rx = 0;
    x.size = OG_W * 2;
    x.flags = 0x100000u;
    while (seen < OG_H && DWT_CYCCNT - t0 < FLUSH_CYCLES) {
        if (y >= OG_H)
            y = 0;
        if (!(s->pend[y >> 5] & (1u << (y & 31)))) {
            y++;
            seen++;
            continue;
        }
        fw_eve_cs_low();
        fw_eve_pause();
        fw_eve_wr_addr(OG_FB_RAMG + y * OG_W * 2);
        while (y < OG_H && (s->pend[y >> 5] & (1u << (y & 31))) && DWT_CYCCNT - t0 < FLUSH_CYCLES) {
            const unsigned short *p = px + y * OG_W;
            s->pend[y >> 5] &= ~(1u << (y & 31));
            for (i = 0; i < OG_W; i++) {    /* RAM_G : RGB565 petit-boutiste */
                u32 c = p[i];
                if (s->fb_swap)
                    c = ((c >> 8) & 0xff) | ((c & 0xff) << 8);
                row[2 * i] = (u8)c;
                row[2 * i + 1] = (u8)(c >> 8);
            }
            x.tx = row;
            fw_lpspi_xfer(base, &x);
            y++;
            seen++;
            sent++;
        }
        fw_eve_cs_high();
        fw_eve_pause();
    }
    s->flush_y = y >= OG_H ? 0 : y;
    if (sent && !s->bitmap) {
        s->bitmap = 1;                  /* première image : la liste d'affichage montre le bitmap */
        s->dirty = 1;
    }
}

static int ok_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-' ||
           c == '_';
}

/* « /Guests/<nom de l'invité>/<name> » ; 0 si un nom ne convient pas. dir_only : sans « /<name> ». */
static int guest_path(char *out, const char *name, int dir_only)
{
    const struct pgi_header *h = og_pgi(own());
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
    u32 total = 0;
    int n;
    if (!guest_path(path, name, 0))
        return -1;
    zero(f, FILE_OBJ);
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
    int n;
    if (!guest_path(path, name, 1))
        return -1;
    fw_f_mkdir(PGI_DIR);                /* 0 créé, 8 existe déjà : dans les deux cas on continue */
    fw_f_mkdir(path);
    if (!guest_path(path, name, 0))
        return -1;
    zero(f, FILE_OBJ);
    if (!fw_file_open(f, path, FA_WRITE_NEW))
        return -1;
    n = len ? fw_file_write(f, src, len) : 0;
    fw_file_close(f);
    return n == (int)len ? 0 : -1;
}

static const struct pgi_host host = {PGI_ABI, host_millis, host_led, host_screen, host_file_read, host_file_write};

/* ---- chargement ------------------------------------------------------------------------------- */

static int entry_ok(const void *fn, u32 base, u32 end)
{
    u32 a = (u32)fn;
    return fn == 0 || ((a & 1) && a > base && a < end);
}

static void mem_prepare(u32 base, u32 sizebits, u32 end)
{
    u32 a;
    dsb();
    MPU_RNR = PGI_REGION;
    MPU_RBAR = base;
    MPU_RASR = RASR_NORMAL_WB | ((sizebits - 1) << 1);
    dsb();
    isb();
    for (a = base & ~31u; a < end; a += 32)
        SCB_DCCMVAC = a;
    dsb();
    SCB_ICIALLU = 0;
    dsb();
    isb();
}

static int load_path(struct own *s, const char *path)
{
    u8 f[FILE_OBJ] __attribute__((aligned(4)));
    u32 hd[sizeof(struct pgi_header) / 4];
    const struct pgi_header *h0 = (const struct pgi_header *)hd, *h;
    u32 base, limit, sizebits, total = 0, i;
    int n;

    zero(f, FILE_OBJ);
    if (!fw_file_open(f, path, FA_READ))
        return 0;
    n = fw_file_read(f, hd, sizeof hd);
    fw_file_close(f);
    if (n != (int)sizeof hd || h0->magic != PGI_MAGIC || h0->abi < 1 || h0->abi > PGI_ABI) {
        s->msg = "invite illisible";
        return 0;
    }
    base = h0->abi >= 2 ? h0->base : PGI_BASE;
    if (base == PGI_BASE) {
        limit = PGI_LIMIT;
        sizebits = 19;
    } else if (base == PGI_SDRAM_BASE) {
        u32 pool_end = fw_sample_pool[0] + 2 * fw_sample_pool[1];
        if (fw_sample_pool[2] > PGI_SDRAM_BASE || pool_end < PGI_SDRAM_BASE + PGI_SDRAM_LIMIT) {
            s->msg = "refuse : samples du projet";
            return 0;
        }
        limit = PGI_SDRAM_LIMIT;
        sizebits = 21;
    } else {
        s->msg = "invite : adresse inconnue";
        return 0;
    }
    zero(f, FILE_OBJ);
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
        !entry_ok((const void *)h->event, base, h->image_end) || !entry_ok((const void *)h->tick, base, h->image_end)) {
        s->msg = "invite abime";
        return 0;
    }
    for (i = h->image_end; i < h->bss_end; i += 4)
        *(volatile u32 *)i = 0;
    mem_prepare(base, sizebits, h->bss_end);
    return (int)base;
}

int og_load(struct own *s)
{
    int base = load_path(s, OWN_GUEST_PATH);
    const struct pgi_header *h;
    if (!base)
        base = load_path(s, PGI_PATH);
    if (!base) {
        /* Le système de fichiers de Polyend garde en mémoire le dernier secteur lu (celui de la racine,
         * ici). Si la carte a changé depuis (fichier posé au Play+ virtuel après l'instantané de
         * démarrage, ou carte échangée), il ne voit pas /Guests. Ouvrir un chemin dans un autre dossier
         * lui fait lire un autre secteur : l'essai suivant (RETRY_MS) relit la racine pour de bon. */
        u8 f[FILE_OBJ] __attribute__((aligned(4)));
        zero(f, FILE_OBJ);
        if (fw_file_open(f, "/Firmware/-", FA_READ))
            fw_file_close(f);
        return 0;
    }
    h = (const struct pgi_header *)base;
    *(volatile u32 *)0xE000EDFCu |= 1u << 24;  /* DEMCR.TRCENA puis DWT_CTRL.CYCCNTENA : le compteur de */
    *(volatile u32 *)0xE0001000u |= 1u;         /* cycles de la charge CPU (déjà actif chez Polyend, au cas où) */
    s->ready = 0;                       /* l'interruption audio attend la fin d'init (silence) */
    s->pgi = (u32)base;                 /* guest_path (fichiers de l'invité) en a besoin pendant init */
    if (h->init)
        h->init(&host);
    s->bands_ms = fw_millis;
    __asm__ volatile("dmb" ::: "memory");
    s->ready = 1;                       /* og_audio peut appeler l'invité */
    s->msg = 0;
    return 1;
}

/* ---- boucle, événements, son -------------------------------------------------------------------- */

void og_event(struct own *s, u32 kind, u32 a, u32 b)
{
    const struct pgi_header *h = og_pgi(s);
    if (h && h->event)
        h->event(kind, a, b);
}

void og_tick(struct own *s)
{
    const struct pgi_header *h = og_pgi(s);
    if (h && h->tick)
        h->tick();
}

void og_audio(struct own *s, s16 *l, s16 *r)
{
    const struct pgi_header *h = og_pgi(s);
    u32 t0, dt;
    if (!h || !h->audio || !s->ready) {
        u32 i;
        for (i = 0; i < 128; i++)       /* pas d'invité, ou init pas finie : silence, pas le bloc d'avant */
            l[i] = r[i] = 0;
        return;
    }
    t0 = DWT_CYCCNT;
    h->audio(l, r, 128);
    dt = DWT_CYCCNT - t0;
    s->cpu_sum += dt;
    if (dt > s->cpu_peak)
        s->cpu_peak = dt;
    if (++s->cpu_n >= CPU_WINDOW) {
        s->cpu_avg_done = s->cpu_sum / CPU_WINDOW;  /* une division par seconde : négligeable */
        s->cpu_peak_done = s->cpu_peak;
        s->cpu_sum = 0;
        s->cpu_n = 0;
        s->cpu_peak = 0;
    }
}

/* ---- image -------------------------------------------------------------------------------------- */

#define CMD_DLSTART 0xffffff00u
#define CMD_SWAP 0xffffff01u
#define DL_DISPLAY 0x00000000u
#define DL_CLEAR_RGB(c) (0x02000000u | ((c) & 0xffffff))
#define DL_CLEAR_ALL 0x26000007u
#define DL_RGB(c) (0x04000000u | ((c) & 0xffffff))
#define DL_END 0x21000000u
#define DL_VFORMAT_1PX 0x27000000u
#define DL_V(x, y) (0x40000000u | (((u32)(x) & 0x7fff) << 15) | ((u32)(y) & 0x7fff))
#define BMP_HANDLE 7
#define FONT_SMALL 8
#define OPT_CENTERX 512
#define C_TEXT 0xe9e9e6
#define C_DIM 0x8a9099

static void put(u32 w)
{
    fw_eve_word(w);
    fw_eve_wp = (u16)((fw_eve_wp + 4) & 0xfff);
    if (fw_eve_wp == 0) {
        fw_eve_end();
        fw_eve_begin();
    }
}

static void text(int x, int y, u32 color, const char *str)
{
    u32 n = 0;
    put(DL_RGB(color));
    while (str[n])
        n++;
    if (fw_eve_wp + 16 + n + 4 >= 0x1000) {
        while (fw_eve_wp)
            put(DL_RGB(color));
    }
    fw_eve_text((u32)x & 0xffff, y, FONT_SMALL, OPT_CENTERX, str, 0);
}

static int band_ok(const struct pgi_header *h, u32 p)
{
    return p >= h->base && p < h->bss_end;
}

static void pct(char *t, u32 cycles)
{
    char *q = fmt_tenths(t, cycles / (BLOCK_BUDGET / 1000u));
    *q++ = '%';
    *q = 0;
}

void og_draw(struct own *s)
{
    const struct pgi_header *h = og_pgi(s);
    char t[16];
    u32 i, k;
    fw_eve_begin();
    put(CMD_DLSTART);
    put(DL_CLEAR_RGB(0));
    put(DL_CLEAR_ALL);
    put(DL_VFORMAT_1PX);
    if (s->bitmap) {
        put(0x05000000u | BMP_HANDLE);                                  /* BITMAP_HANDLE */
        put(0x01000000u | OG_FB_RAMG);                                  /* BITMAP_SOURCE */
        put(0x07000000u | (7u << 19) | ((OG_W * 2) << 9) | OG_H);      /* BITMAP_LAYOUT RGB565 */
        put(0x28000000u);                                               /* BITMAP_LAYOUT_H */
        put(0x08000000u | (OG_W << 9) | OG_H);                          /* BITMAP_SIZE NEAREST, BORDER */
        put(0x29000000u);                                               /* BITMAP_SIZE_H */
        put(DL_RGB(0xffffff));
        put(0x1f000001u);                                               /* BEGIN(BITMAPS) */
        put(DL_V((320 - (int)OG_W) / 2, 0));
        put(DL_END);
    }
    if (h && h->abi >= 4 && band_ok(h, (u32)h->bands)) {                /* bandes de l'invité */
        for (i = 0; i < 16; i++) {
            const char *src = h->bands[i];
            if (!band_ok(h, (u32)src))
                continue;
            for (k = 0; k < 5 && src[k]; k++)
                t[k] = src[k];
            t[k] = 0;
            if (k)
                text(i < 8 ? 20 : 300, 6 + (int)(i % 8) * 22, i & 1 ? C_TEXT : C_DIM, t);
        }
    }
    text(20, 186, C_DIM, "CPU");
    pct(t, s->cpu_avg_done);
    text(20, 206, C_TEXT, t);
    text(300, 186, C_DIM, "PIC");
    pct(t, s->cpu_peak_done);
    text(300, 206, C_TEXT, t);
    if (s->last_btn) {                  /* dernier bouton reçu, ex. « B7 v » appuyé, « B7 ^ » relâché */
        u32 n = (s->last_btn & 0xff) - 1;
        k = 0;
        t[k++] = 'B';
        if (n >= 10)
            t[k++] = (char)('0' + n / 10);
        t[k++] = (char)('0' + n % 10);
        t[k++] = ' ';
        t[k++] = (s->last_btn >> 8) ? '^' : 'v';
        t[k] = 0;
        text(300, 220, C_DIM, t);
    }
    put(DL_DISPLAY);
    put(CMD_SWAP);
    fw_eve_end();
    s->frames++;
}
