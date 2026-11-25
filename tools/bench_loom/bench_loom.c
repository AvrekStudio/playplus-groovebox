/* Banc de charge de LOOM-8 pour le Cortex-M7 du Play+ (mesuré dans Unicorn par run_loom.py).
 * Reprend l'esprit de tests/host/eng_bench.c de LOOM : synth_prepare + synth_render par blocs, ici de
 * 128 trames (le bloc du Play+). Les cas sont choisis par bench_setup(cas, n). */
#include <stdint.h>
#include <string.h>
#include "synth.h"
#include "fx.h"

#define NB 128
static uint8_t par[NTRK + 1][NTP];
static _Alignas(16) uint8_t smem[SYNTH_BYTES];
static synth_t *S = (synth_t *)smem;
static float L[NB], R[NB], S1[NB], S2[NB], OL[NB], OR_[NB];
static _Alignas(8) uint8_t fxm[2][FXSLOT_BYTES];
static int16_t dly_l[FX_DLY_LEN], dly_r[FX_DLY_LEN], rv[FX_RV_LEN];
volatile float sink;
static int use_fx, kit_trk = -1, kit_step;
static uint32_t blocks;

int bench_npresets(void) { return NPRESETS; }
int bench_preset_engine(int k) { return PRESETS[k].par[TP_ENGINE]; }

static void track_preset(int t, int k, int poly)
{
    preset_sound(par[t], &PRESETS[k]);
    if (poly)
        par[t][TP_VMODE] = VM_POLY;
}

/* cas 0 : rien (silence) ; cas 1 : preset k, n notes tenues ; cas 2 : chanson chargée (voir plus bas) */
void bench_setup(int cas, int k, int n, int fx)
{
    int t, i;
    for (t = 0; t < NTRK; t++)
        sound_default(par[t], EG_KNIT);
    kit_trk = -1;
    use_fx = fx;
    if (cas == 1)
        track_preset(0, k, n > 1);
    if (cas == 2) {   /* « chanson chargée » : kit qui joue + acid + accords de knit + pad de voile */
        int kit = -1, acid = -1, knit = -1, voile = -1;
        for (i = 0; i < NPRESETS; i++) {
            int e = PRESETS[i].par[TP_ENGINE];
            if (e == EG_DRUMLINE && kit < 0) kit = i;
            if (e == EG_ACID && acid < 0) acid = i;
            if (e == EG_KNIT && knit < 0) knit = i;
            if (e == EG_VOILE && voile < 0) voile = i;
        }
        track_preset(0, kit, 0); kit_trk = 0;
        track_preset(1, acid, 0);
        track_preset(2, knit, 1);
        track_preset(3, voile, 1);
    }
    synth_init(S, par);
    synth_tempo(S, 120.0f);
    if (fx) {
        static const uint8_t kd[4] = {64, 64, 64, 64};
        fx_init((fxslot_t *)(void *)fxm[0], dly_l, dly_r, 0);
        fx_init((fxslot_t *)(void *)fxm[1], 0, 0, rv);
        fx_set((fxslot_t *)(void *)fxm[0], FXT_DELAY, kd);
        fx_set((fxslot_t *)(void *)fxm[1], FXT_REVERB, kd);
        fx_tempo((fxslot_t *)(void *)fxm[0], 120.0f);
        fx_tempo((fxslot_t *)(void *)fxm[1], 120.0f);
    }
    if (cas == 1) {
        static const int chord[6] = {48, 55, 60, 64, 67, 72};
        for (i = 0; i < n; i++)
            synth_note_on(S, 0, chord[i], 100, 0, (uint32_t)(i + 1));
    }
    if (cas == 2) {
        synth_note_on(S, 1, 36, 100, 0, 1);
        static const int c4[4] = {48, 55, 60, 64};
        for (i = 0; i < 4; i++) synth_note_on(S, 2, c4[i], 90, 0, (uint32_t)(10 + i));
        for (i = 0; i < 4; i++) synth_note_on(S, 3, c4[i] + 12, 80, 0, (uint32_t)(20 + i));
    }
    blocks = 0;
    kit_step = 0;
}

/* un bloc de 128 trames, comme l'appel audio du Play+ ; le kit reçoit une note toutes les ~11 blocs
 * (croches à 120 BPM : 250 ms = 86 blocs de 128 ; on joue plus dense, en doubles croches) */
void bench_block(void)
{
    if (kit_trk >= 0 && (blocks % 22u) == 0u) {
        static const int pat[8] = {36, 42, 38, 42, 36, 36, 38, 42};
        synth_note_on(S, kit_trk, pat[kit_step & 7], 110, 0, 100u + (uint32_t)kit_step);
        kit_step++;
    }
    memset(L, 0, sizeof L); memset(R, 0, sizeof R); memset(S1, 0, sizeof S1); memset(S2, 0, sizeof S2);
    synth_prepare(S, NB);
    synth_render(S, 0, NVOICE, L, R, S1, S2, NB);
    if (use_fx) {
        fx_process((fxslot_t *)(void *)fxm[0], S1, OL, OR_, NB);
        fx_process((fxslot_t *)(void *)fxm[1], S2, OL, OR_, NB);
    }
    sink += L[5] + R[7] + OL[3];
    blocks++;
}

float bench_peak(void)
{
    float p = 0;
    int i;
    for (i = 0; i < NB; i++) { float v = L[i] < 0 ? -L[i] : L[i]; if (v > p) p = v; }
    return p;
}

void *memset(void *d, int c, size_t n) { unsigned char *p = d; while (n--) *p++ = (unsigned char)c; return d; }
void *memcpy(void *d, const void *s, size_t n) { unsigned char *p = d; const unsigned char *q = s; while (n--) *p++ = *q++; return d; }
void *memmove(void *d, const void *s, size_t n) { unsigned char *p = d; const unsigned char *q = s; if (p < q) while (n--) *p++ = *q++; else { p += n; q += n; while (n--) *--p = *--q; } return d; }
int memcmp(const void *a, const void *b, size_t n) { const unsigned char *p = a, *q = b; for (; n; n--, p++, q++) if (*p != *q) return *p - *q; return 0; }
size_t strlen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }
int strcmp(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return (unsigned char)*a - (unsigned char)*b; }
int strncmp(const char *a, const char *b, size_t n) { for (; n; n--, a++, b++) { if (*a != *b || !*a) return (unsigned char)*a - (unsigned char)*b; } return 0; }
char *strncpy(char *d, const char *s, size_t n) { size_t i = 0; for (; i < n && s[i]; i++) d[i] = s[i]; for (; i < n; i++) d[i] = 0; return d; }
