/* Avrek OS : l'invité (firmware FM-1 porté, ex. LOOM-8) chargé depuis la carte SD et lancé dans notre boucle.
 *
 * Repris du mode invité (guest_load.c, guest_screen.c : notre code), sans l'application Polyend autour :
 *   - CHARGEMENT (boucle principale) : OWN_GUEST_PATH, sinon PGI_PATH. En-tête lu seul d'abord (pgi.h) ;
 *     zone OCRAM ou SDRAM selon l'ABI ; pour la SDRAM (haut de la mémoire des samples de Polyend). Jusqu'à
 *     0.6.0, refus si les samples du projet chargé au démarrage y arrivaient (« refuse : samples du projet » :
 *     un projet de plus de ~19 Mo de samples bloquait les invités de 8 Mo). Depuis 0.6.1 on les écrase :
 *     l'officiel ne joue plus rien tant qu'Avrek OS a la main, et revenir au Play+ d'origine après ça passe
 *     par un redémarrage (s->trashed, voir own.c) qui recharge le projet ; image lue par blocs de 4 Ko ; en-tête et points
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
extern int fw_f_opendir(void *dir, const char *path);
extern int fw_f_readdir(void *dir, void *fno);
extern int fw_f_closedir(void *dir);
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
extern void *fw_operator_new(u32 size);
extern void **volatile fw_eve_spi_dev;

#define BOOT_TXT "/Guests/boot.txt"    /* dernier choix du menu d'allumage */
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
static void flush_blocking(struct own *s)
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

/* ---- envoi de l'image en tâche de fond : eDMA vers le LPSPI3 de l'écran (0.5.0) ----------------------
 * Le pilote de Polyend envoie à l'écran (EVE, LPSPI3 0x4039C000) sans DMA, en attendant chaque transfert :
 * ~30 ms de processeur bloqué par image 240 x 240. Ici, une suite d'au plus DMA_ROWS rangées est convertie
 * dans un tampon (DMA_ROWS x 480 o), puis le canal DMA_CH de l'eDMA la pousse octet par octet dans le TDR du
 * LPSPI3 (requête DMAMUX 16 = LPSPI3 TX ; l'officiel n'emploie que les canaux 0 (SAI1, son) et 4-5 (LPSPI4,
 * LED), relevés au virtuel : DMAMUX CHCFG 0x14, 0x4f, 0x50), pendant que la boucle continue. og_flush
 * regarde à chaque tour si c'est fini (TCD CSR.DONE), referme la transaction (FIFO vide, CS haut) et lance
 * la suite. Avant tout autre accès à l'écran (og_draw), og_dma_wait attend la fin.
 * Sécurité : un transfert qui ne finit pas en DMA_TIMEOUT_MS est arrêté, et l'envoi repasse pour de bon à
 * l'ancienne méthode bloquante (s->dma_off) : au pire, l'image comme en 0.4.0.
 * Le Play+ virtuel modèle cet eDMA de façon minimale (cfw/vdev/periph.py : transfert immédiat). */
#define DMA_CH 7u
#define DMA_ROWS 16u
#define DMA_TIMEOUT_MS 50u
#define DMAMUX_LPSPI3_TX 16u
#define EDMA_CERQ (*(volatile u8 *)0x400E801Au)
#define EDMA_SERQ (*(volatile u8 *)0x400E801Bu)
#define EDMA_CDNE (*(volatile u8 *)0x400E801Cu)
#define TCD(o) (*(volatile u32 *)(0x400E9000u + DMA_CH * 32u + (o)))
#define DMAMUX_CHCFG (*(volatile u32 *)(0x400EC000u + 4u * DMA_CH))
#define SPI_R(b, o) (*(volatile u32 *)((u32)(b) + (o)))
#define SPI_CR 0x10u
#define SPI_SR 0x14u
#define SPI_CFGR1 0x24u
#define CFGR1_NOSTALL (1u << 3)
#define DMA_CHECKS 4u                           /* transferts relus avant de faire confiance à l'eDMA */
#define EDMA_ES (*(volatile u32 *)0x400E8004u)
#define DMA_TXT "/Guests/dma.txt"
#define SPI_DER 0x1Cu
#define SPI_FCR 0x58u
#define SPI_FSR 0x5Cu
#define SPI_TCR 0x60u
#define SPI_TDR 0x64u
#define SR_MBF (1u << 24)

static void spi_idle(void *base)                /* plus rien dans la FIFO d'émission, module au repos */
{
    u32 t0 = fw_millis;
    while (((SPI_R(base, SPI_FSR) & 0x1Fu) || (SPI_R(base, SPI_SR) & SR_MBF)) && fw_millis - t0 < 5u)
        ;
}

/* ---- contrôle du départ (0.5.1) ----------------------------------------------------------------------
 * 0.5.0 sur la machine : écran noir (Vector37), image figée (LOOM), alors que tout passait au virtuel (dont
 * l'eDMA est un modèle minimal). Le DONE de l'eDMA arrivait (pas de repli sur délai), donc les octets
 * sortaient mais n'arrivaient pas justes dans RAM_G. Désormais les DMA_CHECKS premiers transferts sont
 * RELUS dans RAM_G (lecture EVE : 3 octets d'adresse, un octet muet, puis les données) et comparés au tampon :
 * au moindre écart, l'eDMA est abandonné pour la session (envoi bloquant, l'image de 0.4.0, toutes les
 * rangées renvoyées). Le résultat et l'état des registres vont dans /Guests/dma.txt (à lire sur un
 * ordinateur pour la suite du diagnostic). Aussi : NOSTALL coupé pendant le transfert (avec la FIFO vidée
 * entre deux requêtes DMA, un LPSPI en NOSTALL enverrait des trames vides au lieu d'attendre). */
/* ---- 0.5.3 : la panne de 0.5.1 lue dans /Guests/dma.txt (machine, Vector37) ----------------------------
 *   « DELAI DEPASSE, rangees 10+10 controles 01 », TCR 7, CFGR1 1, FCR 0, CR 1, SR 01000702, DMA ES 0,
 *   CSR 1e000008, MUX 80000010.
 * Donc : le 1er transfert (rangées 0-15) est arrivé juste (relu) ; le 2e n'a jamais fini, sans erreur eDMA
 * (ES 0, ni ACTIVE ni DONE). Le LPSPI3 reste occupé (SR.MBF) avec des données en réception (SR.RDF) et la FIFO
 * d'émission non vide (TDF 0) : il s'est bloqué, FIFO de réception pleine (CFGR1.NOSTALL 0 : il attend qu'on la
 * vide), le RXMSK écrit dans TCR n'ayant pas pris (TCR passe par la FIFO, derrière la commande laissée par le
 * pilote). Et surtout : le pilote de Polyend refuse tout transfert tant que MBF est mis (début de
 * fw_lpspi_xfer, 0x600b8bc0) : après le blocage, même l'envoi bloquant ne passait plus. D'où l'écran noir de
 * 0.5.0 et 0.5.1, « repli » compris.
 * Remède : comme le pilote au début de chaque transfert, on remet le LPSPI3 à plat AVANT (MEN coupé, FIFO
 * vidées par CR.RTF/RRF, drapeaux effacés, CFGR1 sans NOSTALL, MEN remis) puis on écrit TCR (8 bits, RXMSK)
 * dans une FIFO vide : il prend effet tout de suite. Les 3 octets d'adresse RAM_G partent DANS le tampon DMA,
 * devant les pixels (plus de fw_eve_wr_addr entre la remise à plat et le DMA). À la fin, ou sur délai, même
 * remise à plat : le pilote retrouve un LPSPI3 libre. */
#define CR_RTF (1u << 8)
#define CR_RRF (1u << 9)
#define SR_W1C 0x3F00u
static void spi_flat(void *base, u32 cfgr1)
{
    SPI_R(base, SPI_CR) &= ~1u;                 /* MEN */
    SPI_R(base, SPI_CFGR1) = cfgr1;             /* (ne change que module arrêté) */
    SPI_R(base, SPI_CR) |= CR_RTF | CR_RRF;     /* FIFO vidées, commande en attente comprise */
    SPI_R(base, SPI_SR) = SR_W1C;
    SPI_R(base, SPI_CR) |= 1u;
}
static u8 *dma_px(struct own *s)                /* les pixels dans le tampon ; les 3 octets d'adresse juste devant */
{
    return (u8 *)(((s->dma_buf + 31u) & ~31u) + 32u);
}

static void eve_read(u32 addr, u8 *out, u32 n)  /* n <= 32 */
{
    u8 h[4] __attribute__((aligned(4)));
    u8 z[32] __attribute__((aligned(4)));
    struct lpspi_xfer x;
    void *base = *fw_eve_spi_dev;
    zero(z, sizeof z);
    h[0] = (u8)((addr >> 16) & 0x3Fu);          /* lecture : bits 23-22 à 00 */
    h[1] = (u8)(addr >> 8);
    h[2] = (u8)addr;
    h[3] = 0;                                   /* octet muet */
    fw_eve_cs_low();
    fw_eve_pause();
    x.tx = h;
    x.rx = 0;
    x.size = 4;
    x.flags = 0x100000u;
    fw_lpspi_xfer(base, &x);
    x.tx = z;
    x.rx = out;
    x.size = n;
    fw_lpspi_xfer(base, &x);
    fw_eve_cs_high();
    fw_eve_pause();
}

static char *put_s(char *o, const char *t)
{
    while (*t)
        *o++ = *t++;
    return o;
}

static char *put_x(char *o, u32 v, u32 digits)
{
    u32 k;
    for (k = digits; k--;)
        *o++ = "0123456789abcdef"[(v >> (4u * k)) & 15u];
    return o;
}

static void dma_report(struct own *s, void *base, const char *what, u32 off, const u8 *want, const u8 *got)
{
    char t[320];
    char *o = t;
    u8 f[FILE_OBJ] __attribute__((aligned(4)));
    u32 i;
    o = put_s(o, "Avrek OS " CFW_VERSION_STRING " : envoi d'image par eDMA : ");
    o = put_s(o, what);
    o = put_s(o, "\r\nrangees ");
    o = put_x(o, s->dma_y0, 2);
    o = put_s(o, "+");
    o = put_x(o, s->dma_n, 2);
    o = put_s(o, " controles ");
    o = put_x(o, s->dma_chk, 2);
    if (want) {
        o = put_s(o, " ecart a ");
        o = put_x(o, off, 4);
        o = put_s(o, "\r\nattendu ");
        for (i = 0; i < 8; i++)
            o = put_x(o, want[i], 2);
        o = put_s(o, "\r\nlu      ");
        for (i = 0; i < 8; i++)
            o = put_x(o, got[i], 2);
    }
    o = put_s(o, "\r\nTCR ");
    o = put_x(o, s->dma_tcr, 8);
    o = put_s(o, " CFGR1 ");
    o = put_x(o, s->dma_cfgr1, 8);
    o = put_s(o, " FCR ");
    o = put_x(o, SPI_R(base, SPI_FCR), 8);
    o = put_s(o, " CR ");
    o = put_x(o, SPI_R(base, SPI_CR), 8);
    o = put_s(o, " SR ");
    o = put_x(o, SPI_R(base, SPI_SR), 8);
    o = put_s(o, "\r\nDMA ES ");
    o = put_x(o, EDMA_ES, 8);
    o = put_s(o, " CSR ");
    o = put_x(o, TCD(0x1Cu), 8);
    o = put_s(o, " MUX ");
    o = put_x(o, DMAMUX_CHCFG, 8);
    o = put_s(o, " CITER ");
    o = put_x(o, TCD(0x14u) >> 16, 4);
    o = put_s(o, " FSR ");
    o = put_x(o, SPI_R(base, SPI_FSR), 8);
    o = put_s(o, "\r\n");
    zero(f, FILE_OBJ);
    if (!fw_file_open(f, DMA_TXT, FA_WRITE_NEW))
        return;
    fw_file_write(f, t, (u32)(o - t));
    fw_file_close(f);
}

static void dma_verify(struct own *s, void *base)
{
    const u8 *buf = dma_px(s);
    u32 len = s->dma_n * OG_W * 2u, offs[2], k, i, y;
    u8 got[32] __attribute__((aligned(4)));
    offs[0] = 0;
    offs[1] = len - 32u;
    for (k = 0; k < 2; k++) {
        eve_read(OG_FB_RAMG + s->dma_y0 * OG_W * 2u + offs[k], got, 32);
        for (i = 0; i < 32 && got[i] == buf[offs[k] + i]; i++)
            ;
        if (i < 32) {
            s->dma_off = 1;                     /* l'envoi bloquant pour la session, et toute l'image à refaire */
            for (y = 0; y < 8; y++)
                s->pend[y] = 0xFFFFFFFFu;
            s->pend[7] &= 0xFFFFu;              /* (rangées 224 à 239) */
            i &= ~7u;
            dma_report(s, base, "ECHEC, envoi bloquant", offs[k] + i, buf + offs[k] + i, got + i);
            return;
        }
    }
    if (++s->dma_chk == DMA_CHECKS)
        dma_report(s, base, "OK", 0, 0, 0);
}

static void dma_end(struct own *s, void *base, int ok)
{
    if (ok)
        spi_idle(base);
    else
        EDMA_CERQ = (u8)DMA_CH;
    SPI_R(base, SPI_DER) &= ~1u;                /* (TDDE) */
    if (!ok)                                    /* l'état bloqué, pour dma.txt, avant la remise à plat */
        dma_report(s, base, "DELAI DEPASSE, envoi bloquant", 0, 0, 0);
    EDMA_CDNE = (u8)DMA_CH;
    spi_flat(base, s->dma_cfgr1);               /* le LPSPI3 libre pour le pilote, même après un blocage */
    SPI_R(base, SPI_TCR) = s->dma_tcr;
    fw_eve_cs_high();
    fw_eve_pause();
    s->dma_busy = 0;
    if (!ok) {
        u32 y;
        s->dma_off = 1;                         /* plus de DMA : l'envoi bloquant, toute l'image à refaire */
        for (y = 0; y < 8; y++)
            s->pend[y] = 0xFFFFFFFFu;
        s->pend[7] &= 0xFFFFu;
        return;
    }
    if (s->dma_chk < DMA_CHECKS)
        dma_verify(s, base);
    if (!s->bitmap) {
        s->bitmap = 1;
        s->dirty = 1;
    }
}

static int dma_poll(struct own *s)              /* 1 : plus rien en cours */
{
    void *base = *fw_eve_spi_dev;
    if (!s->dma_busy)
        return 1;
    if (TCD(0x1Cu) & 0x80u) {                   /* CSR.DONE */
        dma_end(s, base, 1);
        return 1;
    }
    if (fw_millis - s->dma_t0 > DMA_TIMEOUT_MS) {
        dma_end(s, base, 0);
        return 1;
    }
    return 0;
}

void og_dma_wait(struct own *s)
{
    while (!dma_poll(s))
        ;
}

/* la prochaine suite de rangées en attente (au plus DMA_ROWS), à partir de flush_y ; 0 si aucune */
static u32 next_run(struct own *s, u32 *y0)
{
    u32 y = s->flush_y, seen = 0, n = 0;
    while (seen < OG_H && !(s->pend[(y % OG_H) >> 5] & (1u << ((y % OG_H) & 31))))
        y++, seen++;
    if (seen >= OG_H)
        return 0;
    y %= OG_H;
    *y0 = y;
    while (y + n < OG_H && n < DMA_ROWS && (s->pend[(y + n) >> 5] & (1u << ((y + n) & 31))))
        n++;
    return n;
}

void og_flush(struct own *s)
{
    const unsigned short *px = (const unsigned short *)s->fb;
    void *base = *fw_eve_spi_dev;
    u8 *buf;
    u32 y0, n, y, i, a;
    if (!px)
        return;
    if (s->dma_busy && !dma_poll(s))
        return;                                 /* la suite précédente part encore : la boucle continue */
    if (s->dma_off) {
        flush_blocking(s);
        return;
    }
    if (!s->dma_buf) {
        s->dma_buf = (u32)fw_operator_new(DMA_ROWS * OG_W * 2u + 64u);
        if (!s->dma_buf) {
            s->dma_off = 1;
            flush_blocking(s);
            return;
        }
    }
    if (!(n = next_run(s, &y0)))
        return;
    buf = dma_px(s);                            /* (aligné sur les lignes du cache) */
    for (y = y0; y < y0 + n; y++) {             /* RGB565 petit-boutiste, pixels lus maintenant (dernière image) */
        const unsigned short *p = px + y * OG_W;
        u8 *o = buf + (y - y0) * OG_W * 2u;
        s->pend[y >> 5] &= ~(1u << (y & 31));
        for (i = 0; i < OG_W; i++) {
            u32 c = p[i];
            if (s->fb_swap)
                c = ((c >> 8) & 0xff) | ((c & 0xff) << 8);
            o[2 * i] = (u8)c;
            o[2 * i + 1] = (u8)(c >> 8);
        }
    }
    s->flush_y = y0 + n >= OG_H ? 0 : y0 + n;
    a = OG_FB_RAMG + y0 * OG_W * 2u;            /* l'en-tête d'écriture EVE : 0x80 | adresse sur 22 bits */
    buf[-3] = (u8)(0x80u | ((a >> 16) & 0x3Fu));
    buf[-2] = (u8)(a >> 8);
    buf[-1] = (u8)a;
    for (a = (u32)buf - 32u; a < (u32)buf + n * OG_W * 2u; a += 32u)
        SCB_DCCMVAC = a;                        /* le tampon en mémoire, pas seulement dans le cache */
    dsb();
    spi_idle(base);
    s->dma_tcr = SPI_R(base, SPI_TCR);
    s->dma_cfgr1 = SPI_R(base, SPI_CFGR1);
    s->dma_y0 = y0;
    s->dma_n = n;
    spi_flat(base, s->dma_cfgr1 & ~CFGR1_NOSTALL);
    fw_eve_cs_low();
    fw_eve_pause();
    {
        volatile u32 rxmsk = 19u;               /* (1 << 19 calculé : la constante 0x80007 ressemble à une adresse
                                                 * de l'ITCM pour le vérificateur) */
        SPI_R(base, SPI_TCR) = (s->dma_tcr & ~0xFFFu) | 7u | (1u << rxmsk);   /* 8 bits, réception ignorée */
    }
    TCD(0x00u) = (u32)buf - 3u;                 /* SADDR : l'en-tête puis les pixels */
    TCD(0x04u) = 1u;                            /* SOFF 1, ATTR : 8 bits -> 8 bits */
    TCD(0x08u) = 1u;                            /* NBYTES : un octet par requête */
    TCD(0x0Cu) = 0;                             /* SLAST */
    TCD(0x10u) = (u32)base + SPI_TDR;           /* DADDR : le TDR du LPSPI3 */
    {
        volatile u32 hdr = 3u;                  /* (calculé : la constante 0x30008 qu'en tirerait le compilateur
                                                 * ressemble à une adresse pour le vérificateur) */
        u32 cnt = n * OG_W * 2u + hdr;
        TCD(0x14u) = cnt << 16;                 /* DOFF 0, CITER */
        TCD(0x18u) = 0;                         /* DLAST */
        TCD(0x1Cu) = (cnt << 16) | 8u;          /* BITER ; CSR.DREQ (requête coupée à la fin) */
    }
    DMAMUX_CHCFG = 0x80000000u | DMAMUX_LPSPI3_TX;
    dsb();
    s->dma_busy = 1;
    s->dma_t0 = fw_millis;
    SPI_R(base, SPI_DER) |= 1u;                 /* TDDE : le LPSPI3 demande des octets à l'eDMA */
    EDMA_SERQ = (u8)DMA_CH;
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

/* (0.6.2, ABI 5) lecture par morceaux : l'objet fichier vit dans struct own (sur le tas : pas de section
 * inscriptible chez nous), 4 poignées. Base de la lecture de samples depuis la carte, et du banc d'essai. */
static int host_fopen(const char *name)
{
    struct own *s = own();
    char path[64];
    int h;
    if (!s || !guest_path(path, name, 0))
        return -1;
    for (h = 0; h < OWN_NFILE; h++)
        if (!((s->gf_open >> h) & 1u))
            break;
    if (h == OWN_NFILE)
        return -1;
    zero(s->gf[h], OWN_FILE_OBJ);
    if (!fw_file_open(s->gf[h], path, FA_READ))
        return -1;
    s->gf_open |= 1u << h;
    return h;
}

static int host_fread(int h, void *dst, unsigned int n)
{
    struct own *s = own();
    u32 total = 0;
    int k;
    if (!s || h < 0 || h >= OWN_NFILE || !((s->gf_open >> h) & 1u))
        return -1;
    while (total < n) {
        k = fw_file_read(s->gf[h], (u8 *)dst + total, n - total < CHUNK ? n - total : CHUNK);
        if (k < 0)
            return total ? (int)total : -1;
        if (k == 0)
            break;
        total += (u32)k;
    }
    return (int)total;
}

static void host_fclose(int h)
{
    struct own *s = own();
    if (!s || h < 0 || h >= OWN_NFILE || !((s->gf_open >> h) & 1u))
        return;
    fw_file_close(s->gf[h]);
    s->gf_open &= ~(1u << h);
}

static const struct pgi_host host = {PGI_ABI, host_millis, host_led, host_screen, host_file_read, host_file_write,
                                     host_fopen, host_fread, host_fclose};

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
        if (pool_end < PGI_SDRAM_BASE + PGI_SDRAM_LIMIT) {
            s->msg = "refuse : hors des samples";
            return 0;
        }
        if (fw_sample_pool[2] > PGI_SDRAM_BASE)
            s->trashed = 1;             /* (0.6.1) samples du projet écrasés : voir plus haut */
        limit = PGI_SDRAM_LIMIT;
        sizebits = 21;
    } else if (base == PGI_SDRAM8_BASE) {      /* (0.6.0) la grande zone : 8 Mo */
        u32 pool_end = fw_sample_pool[0] + 2 * fw_sample_pool[1];
        if (pool_end < PGI_SDRAM8_BASE + PGI_SDRAM8_LIMIT) {
            s->msg = "refuse : hors des samples";
            return 0;
        }
        if (fw_sample_pool[2] > PGI_SDRAM8_BASE)
            s->trashed = 1;             /* (0.6.1) samples du projet écrasés : voir plus haut */
        limit = PGI_SDRAM8_LIMIT;
        sizebits = 23;
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

/* Le système de fichiers de Polyend garde en mémoire le dernier secteur lu (souvent celui de la
 * racine). Si la carte a changé depuis (fichier posé au Play+ virtuel après l'instantané de démarrage,
 * carte échangée), il ne voit pas le nouveau /Guests. Ouvrir un chemin dans un autre dossier lui fait
 * lire un autre secteur : la recherche suivante relit la racine pour de bon. */
static void fs_refresh(void)
{
    u8 f[FILE_OBJ] __attribute__((aligned(4)));
    zero(f, FILE_OBJ);
    if (fw_file_open(f, "/Firmware/-", FA_READ))
        fw_file_close(f);
}

#define DIR_SIZE 0x60                   /* FatFs DIR / FILINFO de l'officiel (guest_pick.c) */
#define FNO_SIZE 0x120
#define FNO_ATTR 0x0c
#define FNO_NAME 0x1a
#define AM_HID 0x02
#define AM_DIR 0x10

static char lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

u32 og_scan(struct own_menu *m)
{
    u8 dir[DIR_SIZE] __attribute__((aligned(4)));
    u8 fno[FNO_SIZE] __attribute__((aligned(4)));
    u32 i, n;
    m->n = 0;
    fs_refresh();
    if (fw_f_opendir(dir, "/Guests") != 0)
        return 0;
    while (m->n < MENU_MAX && fw_f_readdir(dir, fno) == 0) {
        const char *nm = (const char *)fno + FNO_NAME;
        if (!nm[0])
            break;
        for (n = 0; nm[n]; n++)
            ;
        if ((fno[FNO_ATTR] & (AM_DIR | AM_HID)) || n <= 4 || n >= MENU_NAME || nm[n - 4] != '.' ||
            lower(nm[n - 3]) != 'p' || lower(nm[n - 2]) != 'g' || lower(nm[n - 1]) != 'i')
            continue;
        for (i = 0; i <= n; i++)
            m->names[m->n][i] = nm[i];
        m->n++;
    }
    fw_f_closedir(dir);
    return m->n;
}

/* (0.6.3) Mise à jour du firmware sans passer par l'officiel. Ce que fait le menu officiel Settings > Firmware >
 * Update Firmware (relevé au Play+ virtuel le 27/11/2026, vdev_own_maj) : il liste dans /Firmware les fichiers
 * dont le nom commence par « PlayPlus » et finit par « ppf » ; après « Yes », la fonction de l'officiel en
 * 0x600b11f8 (table vers 0x600770dc ; chaîne "/Firmware/_fwinfo" en 0x600770c0) écrit le NOM du fichier (sans
 * chemin, sans fin de ligne) dans /Firmware/_fwinfo, puis redémarre (SYSRESETREQ, boucle en 0x600b11d4). C'est le
 * chargeur de Polyend (sous 0x60030000, jamais touché) qui lit ce fichier au démarrage et installe l'image. On fait
 * pareil : même filtre, même contenu, puis le redémarrage du menu (own.c reboot). */
#define FW_DIR "/Firmware"
#define FW_INFO "/Firmware/_fwinfo"
u32 og_fw_scan(struct own_menu *m)
{
    u8 dir[DIR_SIZE] __attribute__((aligned(4)));
    u8 fno[FNO_SIZE] __attribute__((aligned(4)));
    u32 i, n;
    m->nf = 0;
    fs_refresh();
    if (fw_f_opendir(dir, FW_DIR) != 0)
        return 0;
    while (m->nf < FW_MAX && fw_f_readdir(dir, fno) == 0) {
        const char *nm = (const char *)fno + FNO_NAME;
        if (!nm[0])
            break;
        for (n = 0; nm[n]; n++)
            ;
        if ((fno[FNO_ATTR] & (AM_DIR | AM_HID)) || n <= 11 || n >= FW_NAME || lower(nm[n - 3]) != 'p' ||
            lower(nm[n - 2]) != 'p' || lower(nm[n - 1]) != 'f')
            continue;
        for (i = 0; i < 8 && nm[i] == "PlayPlus"[i]; i++)
            ;
        if (i < 8)
            continue;
        for (i = 0; i <= n; i++)
            m->fw[m->nf][i] = nm[i];
        m->nf++;
    }
    fw_f_closedir(dir);
    return m->nf;
}

int og_fw_request(const char *name)
{
    u8 f[FILE_OBJ] __attribute__((aligned(4)));
    u32 n = 0;
    int w;
    while (name[n])
        n++;
    zero(f, FILE_OBJ);
    if (!n || !fw_file_open(f, FW_INFO, FA_WRITE_NEW))
        return 0;
    w = fw_file_write(f, name, n);
    fw_file_close(f);
    return w == (int)n;
}

int og_last(char *out, u32 max)
{
    u8 f[FILE_OBJ] __attribute__((aligned(4)));
    int n;
    zero(f, FILE_OBJ);
    if (!fw_file_open(f, BOOT_TXT, FA_READ))
        return 0;
    n = fw_file_read(f, out, max - 1);
    fw_file_close(f);
    if (n < 0)
        n = 0;
    out[n] = 0;
    return n;
}

void og_last_set(const char *name)
{
    u8 f[FILE_OBJ] __attribute__((aligned(4)));
    u32 n = 0;
    char old[MENU_NAME + 1];
    int k = og_last(old, sizeof old), i;
    while (name[n])
        n++;
    if (k == (int)n) {                  /* même choix : pas d'écriture sur la carte */
        for (i = 0; i < k && old[i] == name[i]; i++)
            ;
        if (i == k)
            return;
    }
    zero(f, FILE_OBJ);
    if (!fw_file_open(f, BOOT_TXT, FA_WRITE_NEW))
        return;
    fw_file_write(f, name, n);
    fw_file_close(f);
}

/* (0.6.1) Arrête l'invité sans redémarrer, pour revenir au menu d'allumage et en lancer un autre. L'invité
 * n'a rien d'installé hors de sa zone (tout passe par l'hôte) : il suffit que l'interruption audio, la boucle
 * et l'écran ne l'appellent plus. Le prochain chargement remet sa zone à neuf (image relue, .bss à zéro). */
void og_unload(struct own *s)
{
    u32 r0, t0, i;
    s->ready = 0;                       /* l'interruption audio ne l'appelle plus (silence)... */
    __asm__ volatile("dmb" ::: "memory");
    r0 = s->renders;
    t0 = fw_millis;
    while (s->renders - r0 < 2 && fw_millis - t0 < 20)
        ;                               /* ...et l'appel en cours, s'il y en avait un, est fini */
    og_dma_wait(s);                     /* l'image en cours d'envoi */
    s->pgi = 0;
    s->bitmap = 0;
    s->fb = 0;
    s->fb_swap = 0;
    s->flush_y = 0;
    for (i = 0; i < 8; i++)
        s->pend[i] = 0;
    for (i = 0; i < OG_H; i++)
        s->row_sum[i] = 0;
    s->cpu_sum = s->cpu_n = s->cpu_peak = 0;
    s->cpu_avg_done = s->cpu_peak_done = 0;
    s->last_btn = 0;
    for (i = 0; i < OWN_NFILE; i++)      /* (0.6.2) fichiers laissés ouverts par l'invité */
        host_fclose((int)i);
}

int og_load(struct own *s, const char *path)
{
    int base = load_path(s, path);
    const struct pgi_header *h;
    if (!base)
        return 0;
    h = (const struct pgi_header *)base;
    *(volatile u32 *)0xE000EDFCu |= 1u << 24;  /* DEMCR.TRCENA puis DWT_CTRL.CYCCNTENA : le compteur de */
    *(volatile u32 *)0xE0001000u |= 1u;         /* cycles de la charge CPU (déjà actif chez Polyend, au cas où) */
    s->ready = 0;                       /* l'interruption audio attend la fin d'init (silence) */
    {   /* 0.5.2 : l'eDMA de l'image seulement si /Guests/dma.on existe. Sur la machine, 0.5.0 et 0.5.1 donnaient
         * un écran noir (cause pas encore trouvée, voir JOURNAL) : par défaut, l'envoi bloquant de 0.4.0. */
        u8 f[FILE_OBJ] __attribute__((aligned(4)));
        zero(f, FILE_OBJ);
        s->dma_off = 1;
        if (fw_file_open(f, "/Guests/dma.on", FA_READ)) {
            fw_file_close(f);
            s->dma_off = 0;
        }
    }
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
    og_dma_wait(s);                             /* l'écran est à nous seuls : l'image d'abord */
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
