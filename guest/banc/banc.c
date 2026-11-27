/* Banc d'essai du Play+ (AvrekStudio, 27/11/2026) : un invité PGI (ABI 5, grande zone SDRAM) qui mesure la machine
 * réelle et écrit /Guests/BANC/rapport.txt. Pour docs/PLATEFORME.md (points « À mesurer »). Code à nous.
 *
 * Déroulé (tout seul, sauf la fin) :
 *   1. PROCESSEUR et MEMOIRE (dans un seul passage de la boucle : la boucle est bloquée ~2 s, le son continue) :
 *      calcul flottant simple / double, entier ; débit de lecture et d'écriture et latence (pointeurs chaînés) en
 *      cache, en OCRAM et en SDRAM ; exécution d'un noyau de 64 Ko (plus que le cache d'instructions) depuis la
 *      SDRAM puis depuis l'OCRAM (/Guests/BANC/etape.txt écrit avant : un plantage se verra au lancement suivant).
 *   2. SON AU REPOS (5 s) : écart entre deux appels audio (normal : 2,902 ms), histogramme, retards.
 *   3. SON + ECRAN (5 s) : l'image entière redessinée à chaque passage ; écart des appels audio et des passages.
 *   4. CARTE SD : écrire / relire 1 Mo d'un bloc (et vérifier), 32 fichiers de 32 Ko, lecture par morceaux de
 *      4 Ko et de 32 Ko d'un fichier gardé ouvert (débit, pires temps d'attente) ; le son est suivi pendant ce temps.
 *   5. COMMANDES (à toi, PLAY pour passer à la suite, 60 s au plus chacune) : un potard d'une butée à l'autre ; des
 *      pads doucement puis fort (le 3e argument du pilote : vélocité ?) ; effleurer des potards ; un tour d'encodeur.
 * Le rapport est écrit sur la carte après chaque étape (s'il plante, on garde le début). */
#include <stdint.h>
#include "pgi_guest.h"
#include "banc_font.h"

#define DWT (*(volatile uint32_t *)0xE0001004u)
#define CYC_MS 600000u                          /* cycles par ms à 600 MHz */
#define BLOCK_CYC 1741497u                      /* un bloc audio de 128 trames */
static const struct pgi_host *host;

/* ---- texte : rapport, écran ------------------------------------------------------------------------------- */
static char rep[24576];
static uint32_t rlen;
static uint16_t fb[240 * 240];
static char title[32];
static uint32_t scr_dirty;

static void r_c(char c)
{
    if (rlen < sizeof rep - 1)
        rep[rlen++] = c;
    scr_dirty = 1;
}
static void r_s(const char *s)
{
    while (*s)
        r_c(*s++);
}
static void r_u(uint32_t v)
{
    char t[12];
    int k = 0;
    do
        t[k++] = (char)('0' + v % 10u), v /= 10u;
    while (v);
    while (k)
        r_c(t[--k]);
}
static void r_i(int32_t v)
{
    if (v < 0)
        r_c('-'), v = -v;
    r_u((uint32_t)v);
}
static void r_f(uint32_t v100)                  /* centièmes -> « 12.34 » */
{
    r_u(v100 / 100u);
    r_c('.');
    r_c((char)('0' + v100 / 10u % 10u));
    r_c((char)('0' + v100 % 10u));
}
static void r_cyc_ms(uint32_t cyc)              /* cycles -> ms, 2 décimales */
{
    r_f((uint32_t)((uint64_t)cyc * 100u / CYC_MS));
}
static void r_nl(void) { r_c('\n'); }
static void set_title(const char *s)
{
    uint32_t i;
    for (i = 0; s[i] && i < sizeof title - 1; i++)
        title[i] = s[i];
    title[i] = 0;
    scr_dirty = 1;
}

static void glyph(int x, int y, char c, int sc, uint16_t col)
{
    int r, k, i, j;
    if (c >= 'a' && c <= 'z')
        c = (char)(c - 32);
    if (c < 32 || c > 95)
        c = '?';
    for (r = 0; r < 7; r++)
        for (k = 0; k < 5; k++)
            if (FONT57[c - 32][r] & (1u << (4 - k)))
                for (j = 0; j < sc; j++)
                    for (i = 0; i < sc; i++) {
                        int px = x + k * sc + i, py = y + r * sc + j;
                        if (px >= 0 && px < 240 && py >= 0 && py < 240)
                            fb[py * 240 + px] = col;
                    }
}
static void text(int x, int y, const char *s, int sc, uint16_t col)
{
    for (; *s && x < 240; s++, x += 6 * sc)
        glyph(x, y, *s, sc, col);
}

/* l'écran : le titre en grand, puis la fin du rapport (les 26 dernières lignes, 39 caractères) */
static void draw(void)
{
    uint32_t i, n = 0, start = rlen, line = 0, k = 0;
    char buf[40];
    for (i = 0; i < 240 * 240; i++)
        fb[i] = 0;
    text(4, 4, title, 2, 0xFE60);               /* orange */
    while (start > 0 && n < 27)                 /* le début des 26 dernières lignes */
        if (rep[--start] == '\n')
            n++;
    if (n == 27)
        start++;
    for (i = start; i <= rlen && line < 26; i++) {
        if (i == rlen || rep[i] == '\n') {
            buf[k] = 0;
            text(2, 24 + (int)line * 8, buf, 1, 0xE71C);
            line++;
            k = 0;
        } else if (k < 39)
            buf[k++] = rep[i];
    }
    host->screen(fb, 0, 240, 0);
    scr_dirty = 0;
}

static void save(void)
{
    host->file_write("rapport.txt", rep, rlen);
}
static void mark(const char *what)              /* étape en cours, sur la carte (plantage : visible au prochain lancement) */
{
    uint32_t n = 0;
    while (what[n])
        n++;
    host->file_write("etape.txt", what, n);
}

/* ---- son : l'écart entre deux appels audio, par phase ---------------------------------------------------- */
#define NPH 8
#define HB 48                                   /* histogramme : 2,0 .. 4,4 ms par 0,05 ms */
struct astat {
    uint32_t n, min, max, c30, c32, c35, c40, hist[HB];
};
static struct astat ast[NPH];
static volatile uint32_t a_last, a_phase;

static void audio(short *l, short *r, unsigned int n)
{
    uint32_t t = DWT, i;
    if (a_last) {
        struct astat *st = &ast[a_phase % NPH];
        uint32_t d = t - a_last, b;
        st->n++;
        if (d > st->max)
            st->max = d;
        if (!st->min || d < st->min)
            st->min = d;
        st->c30 += d > 3u * CYC_MS;
        st->c32 += d > 32u * CYC_MS / 10u;
        st->c35 += d > 35u * CYC_MS / 10u;
        st->c40 += d > 4u * CYC_MS;
        b = d < 2u * CYC_MS ? 0u : (d - 2u * CYC_MS) / (CYC_MS / 20u);
        st->hist[b < HB ? b : HB - 1u]++;
    }
    a_last = t;
    for (i = 0; i < n; i++)
        l[i] = r[i] = 0;
}

/* écart des passages de la boucle (tick), par phase */
struct tstat {
    uint32_t n, max, sum_ms;
};
static struct tstat tst[NPH];
static uint32_t t_last;

static void report_audio(uint32_t ph, const char *name)
{
    struct astat *st = &ast[ph];
    uint32_t i, med = 0, acc = 0;
    for (i = 0; i < HB; i++) {
        acc += st->hist[i];
        if (acc * 2u >= st->n) {
            med = i;
            break;
        }
    }
    r_s("SON ");
    r_s(name);
    r_s(": ");
    r_u(st->n);
    r_s(" appels, ecart min ");
    r_cyc_ms(st->min);
    r_s(" median ~");
    r_f(200u + med * 5u);
    r_s(" max ");
    r_cyc_ms(st->max);
    r_s(" ms");
    r_nl();
    r_s("  >3.0ms ");
    r_u(st->c30);
    r_s("  >3.2 ");
    r_u(st->c32);
    r_s("  >3.5 ");
    r_u(st->c35);
    r_s("  >4.0 ");
    r_u(st->c40);
    r_nl();
    r_s("  histo 2.0-4.4ms /0.05:");
    for (i = 0; i < HB; i++)
        if (st->hist[i]) {
            r_c(' ');
            r_f(200u + i * 5u);
            r_c('=');
            r_u(st->hist[i]);
        }
    r_nl();
    if (tst[ph].n) {
        r_s("  boucle: ");
        r_u(tst[ph].n);
        r_s(" passages, ecart max ");
        r_u(tst[ph].max);
        r_s(" ms");
        r_nl();
    }
}

/* ---- 1. processeur et mémoire ---------------------------------------------------------------------------- */
static uint32_t big[1u << 20] __attribute__((aligned(32)));   /* 4 Mo en SDRAM (notre .bss) */
#define OCRAM_DATA ((uint32_t *)0x20200000u)    /* OCRAM2, vide tant qu'aucun invité OCRAM n'est chargé */
#define OCRAM_CODE 0x20240000u
static volatile uint32_t sink;
extern char banc_kernel[], banc_kernel_end[];
typedef uint32_t (*kfn)(uint32_t, uint32_t);

static uint32_t __attribute__((noinline)) f32_mac(uint32_t n)
{
    float a0 = 1, a1 = 1, a2 = 1, a3 = 1, a4 = 1, a5 = 1, a6 = 1, a7 = 1, m = 0.9999f, c = 0.0001f;
    uint32_t i, t = DWT;
    for (i = 0; i < n; i++) {
        a0 = a0 * m + c, a1 = a1 * m + c, a2 = a2 * m + c, a3 = a3 * m + c;
        a4 = a4 * m + c, a5 = a5 * m + c, a6 = a6 * m + c, a7 = a7 * m + c;
    }
    t = DWT - t;
    sink = (uint32_t)(a0 + a1 + a2 + a3 + a4 + a5 + a6 + a7);
    return t;
}
static uint32_t __attribute__((noinline)) f64_mac(uint32_t n)
{
    double a0 = 1, a1 = 1, a2 = 1, a3 = 1, a4 = 1, a5 = 1, a6 = 1, a7 = 1, m = 0.9999, c = 0.0001;
    uint32_t i, t = DWT;
    for (i = 0; i < n; i++) {
        a0 = a0 * m + c, a1 = a1 * m + c, a2 = a2 * m + c, a3 = a3 * m + c;
        a4 = a4 * m + c, a5 = a5 * m + c, a6 = a6 * m + c, a7 = a7 * m + c;
    }
    t = DWT - t;
    sink = (uint32_t)(a0 + a1 + a2 + a3 + a4 + a5 + a6 + a7);
    return t;
}
static uint32_t __attribute__((noinline)) i32_mac(uint32_t n)
{
    uint32_t a0 = 1, a1 = 2, a2 = 3, a3 = 4, m = 0x9E3779B1u, c = sink | 1u, i, t = DWT;
    for (i = 0; i < n; i++) {
        a0 = a0 * m + c, a1 = a1 * m + c, a2 = a2 * m + c, a3 = a3 * m + c;
    }
    t = DWT - t;
    sink = a0 + a1 + a2 + a3;
    return t;
}
static uint32_t __attribute__((noinline)) rd_bw(const uint32_t *p, uint32_t words, uint32_t passes)
{
    uint32_t s0 = 0, s1 = 0, s2 = 0, s3 = 0, i, k, t = DWT;
    for (k = 0; k < passes; k++)
        for (i = 0; i < words; i += 4)
            s0 += p[i], s1 += p[i + 1], s2 += p[i + 2], s3 += p[i + 3];
    t = DWT - t;
    sink = s0 + s1 + s2 + s3;
    return t;
}
static uint32_t __attribute__((noinline)) wr_bw(uint32_t *p, uint32_t words, uint32_t passes)
{
    uint32_t i, k, t = DWT;
    for (k = 0; k < passes; k++)
        for (i = 0; i < words; i += 4)
            p[i] = k, p[i + 1] = k, p[i + 2] = k, p[i + 3] = k;
    t = DWT - t;
    return t;
}
static uint32_t __attribute__((noinline)) chase(uint32_t *p, uint32_t words, uint32_t steps)
{
    uint32_t i, x = 0, t, stride = 4099u;       /* saut de ~16 Ko : chaque accès sort de la ligne de cache */
    for (i = 0; i < words; i++)
        p[i] = (i + stride) & (words - 1u);
    __asm__ volatile("dsb" ::: "memory");
    t = DWT;
    for (i = 0; i < steps; i++)
        x = p[x];
    t = DWT - t;
    sink = x;
    return t;
}

static void r_bw(const char *what, uint32_t bytes, uint32_t cyc)
{
    r_s(what);
    r_s(": ");
    r_u((uint32_t)((uint64_t)bytes * 600u / (cyc ? cyc : 1u)));   /* octets/µs = Mo/s */
    r_s(" Mo/s");
    r_nl();
}
static void r_per(const char *what, uint32_t cyc, uint32_t n, const char *unit)
{
    r_s(what);
    r_s(": ");
    r_f((uint32_t)((uint64_t)cyc * 100u / (n ? n : 1u)));
    r_s(unit);
    r_nl();
}

static void dcache_clean(uint32_t a, uint32_t n)
{
    uint32_t e = a + n;
    __asm__ volatile("dsb" ::: "memory");
    for (a &= ~31u; a < e; a += 32u)
        *(volatile uint32_t *)0xE000EF68u = a;  /* DCCMVAC */
    __asm__ volatile("dsb" ::: "memory");
    *(volatile uint32_t *)0xE000EF50u = 0;      /* ICIALLU */
    __asm__ volatile("dsb\n\tisb" ::: "memory");
}

static uint32_t run_kernel(kfn f, uint32_t *best)
{
    uint32_t k, t, worst = 0;
    *best = 0xffffffffu;
    for (k = 0; k < 8; k++) {
        t = DWT;
        sink = f(0, 1);
        t = DWT - t;
        if (t < *best)
            *best = t;
        if (t > worst)
            worst = t;
    }
    return worst;
}

static void step_cpu(void)
{
    uint32_t best, worst, ksz = (uint32_t)(banc_kernel_end - banc_kernel), nins = ksz / 4u;
    r_s("== PROCESSEUR (cycles a 600 MHz) ==");
    r_nl();
    r_per("float x+  (8 chaines)", f32_mac(200000) , 1600000, " cycles/op");
    r_per("double x+ (8 chaines)", f64_mac(200000), 1600000, " cycles/op");
    r_per("entier x+ (4 chaines)", i32_mac(400000), 1600000, " cycles/op");
    r_s("== MEMOIRE ==");
    r_nl();
    r_bw("lecture 16 Ko (cache)", 16384u * 64u, rd_bw(big, 4096, 64));
    r_bw("lecture 4 Mo SDRAM", 4u << 20, rd_bw(big, 1u << 20, 1));
    r_bw("ecriture 4 Mo SDRAM", 4u << 20, wr_bw(big, 1u << 20, 1));
    r_per("latence SDRAM (4 Mo)", chase(big, 1u << 20, 100000), 100000, " cycles/acces");
    mark("2 OCRAM donnees");
    r_bw("lecture 256 Ko OCRAM", 256u << 10, rd_bw(OCRAM_DATA, 65536, 1));
    r_bw("lecture 256 Ko OCRAM x8", (256u << 10) * 8u, rd_bw(OCRAM_DATA, 65536, 8));
    r_bw("ecriture 256 Ko OCRAM", 256u << 10, wr_bw(OCRAM_DATA, 65536, 1));
    r_per("latence OCRAM (256 Ko)", chase(OCRAM_DATA, 65536, 100000), 100000, " cycles/acces");
    r_s("code ");
    r_u(ksz);
    r_s(" o, ");
    r_u(nins);
    r_s(" instructions:");
    r_nl();
    mark("3 code SDRAM");
    worst = run_kernel((kfn)((uint32_t)banc_kernel | 1u), &best);
    r_per("  depuis SDRAM, meilleur", best, nins, " cycles/instr");
    r_per("  depuis SDRAM, pire", worst, nins, " cycles/instr");
    save();
    mark("4 code OCRAM");
    {
        const uint32_t *src = (const uint32_t *)banc_kernel;
        uint32_t *dst = (uint32_t *)OCRAM_CODE, i;
        for (i = 0; i < (ksz + 3u) / 4u; i++)
            dst[i] = src[i];
        dcache_clean(OCRAM_CODE, ksz);
        worst = run_kernel((kfn)(OCRAM_CODE | 1u), &best);
    }
    r_per("  depuis OCRAM, meilleur", best, nins, " cycles/instr");
    r_per("  depuis OCRAM, pire", worst, nins, " cycles/instr");
    mark("5 fin processeur");
}

/* ---- 4. carte SD ---------------------------------------------------------------------------------------- */
static uint32_t lat_hist[9];                    /* <0.5 <1 <2 <5 <10 <20 <50 <100 >=100 ms */
static const char *const LAT_LAB[9] = {"<0.5", "<1", "<2", "<5", "<10", "<20", "<50", "<100", ">=100"};
static void lat_add(uint32_t cyc)
{
    uint32_t us = cyc / 600u, b = us < 500u ? 0 : us < 1000u ? 1 : us < 2000u ? 2 : us < 5000u ? 3 : us < 10000u ? 4
                                   : us < 20000u ? 5 : us < 50000u ? 6 : us < 100000u ? 7 : 8;
    lat_hist[b]++;
}
static void lat_report(void)
{
    uint32_t i;
    r_s("  attente (ms):");
    for (i = 0; i < 9; i++)
        if (lat_hist[i]) {
            r_c(' ');
            r_s(LAT_LAB[i]);
            r_c('=');
            r_u(lat_hist[i]);
        }
    r_nl();
    for (i = 0; i < 9; i++)
        lat_hist[i] = 0;
}

static void name2(char *o, const char *pre, uint32_t i)
{
    while (*pre)
        *o++ = *pre++;
    *o++ = (char)('0' + i / 10u), *o++ = (char)('0' + i % 10u);
    *o++ = '.', *o++ = 'b', *o++ = 'i', *o++ = 'n', *o = 0;
}

static void sd_stream(uint32_t chunk)
{
    int h = host->abi >= 5 && host->fopen ? host->fopen("t1m.bin") : -1;
    uint32_t total = 0, mx = 0, t0 = host->millis(), n = 0;
    uint8_t *buf = (uint8_t *)big;
    if (h < 0) {
        r_s("lecture par morceaux: indisponible (Avrek OS 0.6.2 requis)");
        r_nl();
        return;
    }
    for (;;) {
        uint32_t t = DWT;
        int k = host->fread(h, buf, chunk);
        t = DWT - t;
        if (k <= 0)
            break;
        lat_add(t);
        if (t > mx)
            mx = t;
        total += (uint32_t)k;
        n++;
    }
    host->fclose(h);
    t0 = host->millis() - t0;
    r_s("morceaux de ");
    r_u(chunk / 1024u);
    r_s(" Ko: ");
    r_u(n);
    r_s(" lus, ");
    r_u(t0 ? total / t0 : 0);
    r_s(" Ko/s, pire ");
    r_cyc_ms(mx);
    r_s(" ms");
    r_nl();
    lat_report();
}

static void step_sd(void)
{
    uint8_t *a = (uint8_t *)big, *b = (uint8_t *)big + (1u << 20);
    uint32_t i, t, bad = 0, mx = 0, sum = 0;
    char nm[16];
    for (i = 0; i < (1u << 20); i++)
        a[i] = (uint8_t)(i * 7u + (i >> 9));
    r_s("== CARTE SD ==");
    r_nl();
    draw();
    t = host->millis();
    i = (uint32_t)host->file_write("t1m.bin", a, 1u << 20);
    t = host->millis() - t;
    r_s("ecrire 1 Mo d'un bloc: ");
    r_u(t);
    r_s(" ms");
    r_s(i ? " (ECHEC)" : "");
    r_nl();
    draw();
    t = host->millis();
    i = (uint32_t)host->file_read("t1m.bin", b, 1u << 20);
    t = host->millis() - t;
    for (bad = 0, sum = 0; sum < (1u << 20); sum++)
        bad += a[sum] != b[sum];
    r_s("relire 1 Mo d'un bloc: ");
    r_u(t);
    r_s(" ms, ");
    r_u(i);
    r_s(" o lus, octets faux: ");
    r_u(bad);
    r_nl();
    save();
    draw();
    mx = sum = 0;
    for (i = 0; i < 32; i++) {
        name2(nm, "s", i);
        t = host->millis();
        host->file_write(nm, a + i * 32768u, 32768u);
        t = host->millis() - t;
        sum += t;
        if (t > mx)
            mx = t;
    }
    r_s("ecrire 32 x 32 Ko: moyenne ");
    r_u(sum / 32u);
    r_s(" ms, pire ");
    r_u(mx);
    r_s(" ms");
    r_nl();
    draw();
    mx = sum = 0;
    for (i = 0; i < 64; i++) {
        uint32_t c;
        name2(nm, "s", i % 32u);
        c = DWT;
        host->file_read(nm, b, 32768u);
        c = DWT - c;
        lat_add(c);
        sum += c / 600u;
        if (c > mx)
            mx = c;
    }
    r_s("lire 64 x 32 Ko (fichiers): moyenne ");
    r_f(sum / 64u / 10u);
    r_s(" ms, pire ");
    r_cyc_ms(mx);
    r_s(" ms");
    r_nl();
    lat_report();
    draw();
    sd_stream(4096u);
    draw();
    sd_stream(32768u);
    save();
}

/* ---- 5. commandes -------------------------------------------------------------------------------------- */
static int32_t kn_sum[15], kn_min[15], kn_max[15];
static uint32_t kn_ev[15], kn_absmax[15];
static uint32_t px_n, px_min = 0xffffffffu, px_max, px_list[48], px_len;
static uint32_t tc_n[15], tc_state[4];
static int32_t enc_sum;
static uint32_t enc_ev;
static volatile uint32_t play_hit;

static void event(unsigned int kind, unsigned int a, unsigned int b)
{
    if (kind == PGI_EV_BUTTON && a == 8u && b == 0u)
        play_hit = 1;
    else if (kind == PGI_EV_KNOB && a < 15u) {
        int32_t d = (int32_t)b;
        kn_sum[a] += d;
        kn_ev[a]++;
        if ((uint32_t)(d < 0 ? -d : d) > kn_absmax[a])
            kn_absmax[a] = (uint32_t)(d < 0 ? -d : d);
        if (kn_sum[a] < kn_min[a])
            kn_min[a] = kn_sum[a];
        if (kn_sum[a] > kn_max[a])
            kn_max[a] = kn_sum[a];
    } else if (kind == PGI_EV_PADX) {
        px_n++;
        if (b < px_min)
            px_min = b;
        if (b > px_max)
            px_max = b;
        if (px_len < 48u)
            px_list[px_len++] = (a << 16) | (b & 0xffffu);
    } else if (kind == PGI_EV_TOUCH && a < 15u) {
        tc_n[a]++;
        tc_state[b < 3u ? b : 3u]++;
    } else if (kind == PGI_EV_ENCODER) {
        enc_sum += (int32_t)b;
        enc_ev++;
    }
}

/* ---- déroulé --------------------------------------------------------------------------------------------- */
enum { S_INTRO, S_CPU, S_IDLE, S_SCREEN, S_SD, S_KNOB, S_PADS, S_TOUCH, S_ENC, S_DONE, S_END };
static uint32_t st = S_INTRO, st_ms, frame, draw_ms;
static const char *const BAND_TXT[S_END + 1] = {"INTRO", "CPU", "REPOS", "ECRAN", "SD", "POT", "PADS", "TOUCH",
                                                "ENC", "FINI", "FINI"};
static const char *bands[16] = {"BANC", "-", "ETAPE", "-", 0, 0, 0, 0, "PLAY", "SUITE", 0, 0, 0, 0, 0, 0};

static void go(uint32_t s, uint32_t now)
{
    st = s;
    st_ms = now;
    a_phase = s % NPH;
    bands[1] = BAND_TXT[s];
    play_hit = 0;
}

static void ask(const char *t1, const char *t2)
{
    set_title("A TOI");
    r_s(">> ");
    r_s(t1);
    r_nl();
    r_s("   ");
    r_s(t2);
    r_nl();
    r_s("   puis PLAY (60 s au plus)");
    r_nl();
}

static void tick(void)
{
    uint32_t now = host->millis();
    struct tstat *ts = &tst[st % NPH];
    if (t_last) {
        uint32_t d = now - t_last;
        ts->n++;
        if (d > ts->max)
            ts->max = d;
        ts->sum_ms += d;
    }
    t_last = now;
    switch (st) {                               /* (petit switch : 11 cas, pas de table de saut) */
    case S_INTRO:
        if (now - st_ms > 2500u) {
            set_title("PROCESSEUR");
            draw();
            go(S_CPU, now);
        }
        break;
    case S_CPU:
        step_cpu();
        save();
        set_title("SON AU REPOS 5 S");
        go(S_IDLE, now);
        break;
    case S_IDLE:
        if (now - st_ms > 5000u) {
            report_audio(S_IDLE, "au repos");
            save();
            set_title("SON + ECRAN 5 S");
            go(S_SCREEN, now);
        }
        break;
    case S_SCREEN: {
        uint32_t i;
        frame++;
        for (i = 0; i < 240u * 240u; i++)       /* toute l'image change à chaque passage */
            fb[i] = (uint16_t)((i + frame * 977u) * 0x9E37u >> 8);
        host->screen(fb, 0, 240, 0);
        if (now - st_ms > 5000u) {
            report_audio(S_SCREEN, "+ ecran");
            r_s("  images envoyees: ");
            r_u(frame);
            r_nl();
            save();
            set_title("CARTE SD");
            go(S_SD, now);
            draw();
        }
        return;                                 /* (pas de redessin du texte pendant ce test) */
    }
    case S_SD:
        step_sd();
        report_audio(S_SD, "pendant la carte");
        save();
        ask("tourne le POTARD 1 (haut gauche)", "d'une butee a l'autre, aller-retour");
        go(S_KNOB, now);
        break;
    case S_KNOB:
        if (play_hit || now - st_ms > 60000u) {
            uint32_t k;
            r_s("POTARDS (crans cumules, min..max, evenements, plus grand cran):");
            r_nl();
            for (k = 0; k < 15u; k++)
                if (kn_ev[k]) {
                    r_s("  P");
                    r_u(k + 1u);
                    r_s(": ");
                    r_i(kn_min[k]);
                    r_s("..");
                    r_i(kn_max[k]);
                    r_s(", ");
                    r_u(kn_ev[k]);
                    r_s(" ev, cran max ");
                    r_u(kn_absmax[k]);
                    r_nl();
                }
            save();
            ask("appuie sur des PADS, doucement", "puis tres fort, plusieurs fois");
            go(S_PADS, now);
        }
        break;
    case S_PADS:
        if (play_hit || now - st_ms > 60000u) {
            uint32_t k;
            r_s("PADS: 3e argument du pilote, ");
            r_u(px_n);
            r_s(" valeurs, min ");
            r_u(px_n ? px_min : 0u);
            r_s(" max ");
            r_u(px_max);
            r_nl();
            r_s("  (pad:valeur)");
            for (k = 0; k < px_len; k++) {
                r_c(' ');
                r_u(px_list[k] >> 16);
                r_c(':');
                r_u(px_list[k] & 0xffffu);
            }
            r_nl();
            save();
            ask("EFFLEURE des potards", "sans les tourner");
            go(S_TOUCH, now);
        }
        break;
    case S_TOUCH:
        if (play_hit || now - st_ms > 60000u) {
            uint32_t k;
            r_s("TOUCHER: etats 0=");
            r_u(tc_state[0]);
            r_s(" 1=");
            r_u(tc_state[1]);
            r_s(" 2=");
            r_u(tc_state[2]);
            r_s(" autre=");
            r_u(tc_state[3]);
            r_s(", potards:");
            for (k = 0; k < 15u; k++)
                if (tc_n[k]) {
                    r_s(" P");
                    r_u(k + 1u);
                    r_c('=');
                    r_u(tc_n[k]);
                }
            r_nl();
            save();
            ask("fais UN TOUR COMPLET d'encodeur", "(sous l'ecran), dans un sens");
            go(S_ENC, now);
        }
        break;
    case S_ENC:
        if (play_hit || now - st_ms > 60000u) {
            r_s("ENCODEUR: ");
            r_i(enc_sum);
            r_s(" crans, ");
            r_u(enc_ev);
            r_s(" evenements");
            r_nl();
            r_s("== FIN : /Guests/BANC/rapport.txt ==");
            r_nl();
            save();
            mark("fini");
            set_title("FINI");
            go(S_DONE, now);
        }
        break;
    default:
        break;
    }
    if (scr_dirty && now - draw_ms > 50u) {     /* (le texte : 20 fois par seconde au plus) */
        draw_ms = now;
        draw();
    }
}

static void init(const struct pgi_host *h)
{
    char prev[32];
    int n;
    host = h;
    set_title("BANC D'ESSAI");
    r_s("Banc d'essai Avrek OS (ABI ");
    r_u(h->abi);
    r_s(")");
    r_nl();
    n = h->file_read("etape.txt", prev, sizeof prev - 1);
    if (n > 0 && !(n == 4 && prev[0] == 'f')) {
        prev[n] = 0;
        r_s("! le lancement precedent s'est arrete a: ");
        r_s(prev);
        r_nl();
    }
    r_s("le son est muet pendant tout le banc");
    r_nl();
    mark("1 debut");
    go(S_INTRO, h->millis());
    draw();
}

PGI_GUEST_BANDS("BANC", init, audio, event, tick, bands);
