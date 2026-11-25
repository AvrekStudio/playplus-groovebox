/* Invité de démonstration « demo » : un petit instrument à pads, pour valider le chargement depuis la
 * carte SD (mode invité, jalon M4). Tout est à nous.
 *
 * - Chaque pad joue une note : gamme pentatonique sur les colonnes, une octave par rangée (la rangée du
 *   bas est la plus grave). Voix « pincée » : sinus approché + une harmonique, enveloppe qui décroît.
 * - 8 voix, la plus ancienne est volée.
 * - Potard 0 : durée de la décroissance. Encodeur : transposition par demi-tons.
 * - LED : chaque voix allume son pad, en couleur selon la rangée, avec une luminosité qui suit
 *   l'enveloppe ; les pads tenus restent allumés. */
#include "pgi_guest.h"

#define NV 8
#define SR 44100.0f

static const struct pgi_host *host;

struct voice {
    float ph, inc, env;
    int pad;              /* pad qui a lancé la voix, -1 = libre */
};
static struct voice v[NV];
static unsigned int next_v;
static float decay = 0.99985f;       /* par échantillon */
static int transpose;
static unsigned int held[5];
static unsigned char lit[160];       /* luminosité affichée par pad, 0..255 (pour n'écrire que les changements) */
static unsigned int last_led_ms;

static const int penta[5] = {0, 2, 4, 7, 9};

/* 2^(x/12) sans libm : puissances de 2 entières + approximation polynomiale sur [0, 1[ */
static float semis(int s)
{
    int oct = s >= 0 ? s / 12 : -((-s + 11) / 12);
    float f = (float)(s - oct * 12) / 12.0f;           /* 0..1 */
    float r = 1.0f + f * (0.6931472f + f * (0.2402265f + f * 0.0555041f)); /* 2^f */
    while (oct > 0) { r *= 2.0f; oct--; }
    while (oct < 0) { r *= 0.5f; oct++; }
    return r;
}

/* sinus approché d'une phase 0..1 (parabole corrigée, erreur < 0,1 %) */
static float sine01(float p)
{
    float x = p * 2.0f - 1.0f;                          /* -1..1 */
    float y = 4.0f * x * (1.0f - (x < 0 ? -x : x));     /* parabole */
    y = 0.225f * (y * (y < 0 ? -y : y) - y) + y;
    return -y;
}

static void note_on(int pad)
{
    int row = pad / 20, col = pad % 20;
    int semi = (7 - row) * 12 / 2 + penta[col % 5] + 12 * (col / 5) - 24 + transpose;
    struct voice *w = &v[next_v];
    next_v = (next_v + 1) % NV;
    w->ph = 0;
    w->inc = 220.0f * semis(semi) / SR;
    w->env = 1.0f;
    w->pad = pad;
}

static void init(const struct pgi_host *h)
{
    unsigned int i;
    host = h;
    for (i = 0; i < NV; i++)
        v[i].pad = -1;
}

static void audio(short *l, short *r, unsigned int n)
{
    unsigned int i, k;
    for (i = 0; i < n; i++) {
        float s = 0;
        for (k = 0; k < NV; k++) {
            struct voice *w = &v[k];
            float p2;
            if (w->env < 0.0005f)
                continue;
            p2 = w->ph * 2.0f;
            if (p2 >= 1.0f)
                p2 -= 1.0f;
            s += w->env * (sine01(w->ph) + 0.3f * w->env * sine01(p2));
            w->ph += w->inc;
            if (w->ph >= 1.0f)
                w->ph -= 1.0f;
            w->env *= decay;
        }
        s *= 6000.0f;
        if (s > 32000.0f) s = 32000.0f;
        if (s < -32000.0f) s = -32000.0f;
        l[i] = r[i] = (short)s;
    }
}

static void event(unsigned int kind, unsigned int a, unsigned int b)
{
    if (kind == PGI_EV_PAD && a < 160) {
        if (b == 0) {
            held[a >> 5] |= 1u << (a & 31);
            note_on((int)a);
        } else if (b == 1) {
            held[a >> 5] &= ~(1u << (a & 31));
        }
    } else if (kind == PGI_EV_KNOB && a == 0) {
        decay += (float)(int)b * 0.00002f;
        if (decay > 0.99998f) decay = 0.99998f;
        if (decay < 0.9990f) decay = 0.9990f;
    } else if (kind == PGI_EV_ENCODER) {
        transpose += (int)b;
        if (transpose > 24) transpose = 24;
        if (transpose < -24) transpose = -24;
    }
}

/* couleur d'une rangée (palette à nous) */
static const unsigned char row_rgb[8][3] = {
    {255, 60, 40}, {255, 140, 0}, {230, 220, 0}, {60, 220, 60},
    {0, 200, 180}, {40, 120, 255}, {150, 70, 255}, {255, 60, 180},
};

static void tick(void)
{
    unsigned int i, k, now = host->millis();
    unsigned char want[160];
    if (now - last_led_ms < 30)
        return;
    last_led_ms = now;
    for (i = 0; i < 160; i++)
        want[i] = ((held[i >> 5] >> (i & 31)) & 1) ? 255 : 0;
    for (k = 0; k < NV; k++) {
        if (v[k].pad >= 0) {
            unsigned int e = (unsigned int)(v[k].env * 255.0f);
            if (e > want[v[k].pad])
                want[v[k].pad] = (unsigned char)e;
            if (v[k].env < 0.0005f)
                v[k].pad = -1;
        }
    }
    for (i = 0; i < 160; i++) {
        if (want[i] != lit[i]) {
            const unsigned char *c = row_rgb[i / 20];
            lit[i] = want[i];
            host->led(i, c[0] * want[i] / 255, c[1] * want[i] / 255, c[2] * want[i] / 255);
        }
    }
}

PGI_GUEST("demo", init, audio, event, tick);
