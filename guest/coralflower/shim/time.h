/* CoralFlower sur le Play+ (27/11) : copie de guest/sdk/hostshim/time.h, mais clock_gettime donne le vrai temps (le
 * compteur de cycles du Cortex-M7 à 600 MHz, étendu à 64 bits). Son émulateur web lit fm1_ticks en temps simulé
 * (emu_sim : il n'avance qu'à chaque milliseconde de la boucle, jamais pendant le calcul du son) ; plat_coralflower.c
 * remet emu_sim à 0 après le démarrage : fm1_ticks = clock_gettime en µs. Sans ça, CoralFlower croyait que chaque
 * demi-tampon prenait 0 µs : sa gestion des voix (CF_VLOAD : « autant de voix que le cœur a le temps ») et son
 * délestage (shed_check) ne se déclenchaient jamais. Mise à jour du compteur étendu interruptions masquées (appelé
 * par l'interruption audio et par la boucle ; il faut un appel toutes les 7,16 s au plus : l'audio en fait 344 par s). */
#ifndef HOSTSHIM_TIME_H
#define HOSTSHIM_TIME_H
typedef long time_t;
typedef long clock_t;
struct timespec { long tv_sec, tv_nsec; };
#define CLOCK_MONOTONIC 1
#define CLOCKS_PER_SEC 1000000
static unsigned long cf_cyc_last, cf_cyc_hi;
static inline int hs_clock_gettime(int c, struct timespec *t)
{
    unsigned long pm, cyc, hi;
    unsigned long long all, ns;
    (void)c;
    __asm__ volatile("mrs %0, primask\n\tcpsid i" : "=r"(pm)::"memory");
    cyc = *(volatile unsigned long *)0xE0001004u;
    if (cyc < cf_cyc_last)
        cf_cyc_hi++;
    cf_cyc_last = cyc;
    hi = cf_cyc_hi;
    __asm__ volatile("msr primask, %0" ::"r"(pm) : "memory");
    all = ((unsigned long long)hi << 32) | cyc;
    ns = all * 5u / 3u;                         /* 600 MHz : 5 / 3 ns par cycle */
    t->tv_sec = (long)(ns / 1000000000u);
    t->tv_nsec = (long)(ns % 1000000000u);
    return 0;
}
static inline clock_t hs_clock(void) { return 0; }
static inline time_t hs_time(time_t *t) { if (t) *t = 0; return 0; }
#define clock_gettime hs_clock_gettime
#define clock hs_clock
#define time hs_time
#endif
