/* Invité minimal « bip » : chaque pad fait un bip carré bref (hauteur selon la colonne), sa LED
 * s'allume en blanc le temps du bip. Sert d'exemple court et de second invité pour les tests du
 * sélecteur. */
#include "pgi_guest.h"

static const struct pgi_host *host;
static volatile unsigned int phase, inc, left;   /* left : échantillons restants du bip */
static volatile int lit_pad = -1;
static int shown = -1;

static void init(const struct pgi_host *h)
{
    host = h;
}

static void audio(short *l, short *r, unsigned int n)
{
    unsigned int i;
    for (i = 0; i < n; i++) {
        short s = 0;
        if (left) {
            s = (phase & 0x80000000u) ? 4000 : -4000;
            phase += inc;
            left--;
        }
        l[i] = r[i] = s;
    }
}

static void event(unsigned int kind, unsigned int a, unsigned int b)
{
    if (kind == PGI_EV_PAD && b == 0 && a < 160) {
        inc = 20000000u + (a % 20) * 4000000u;    /* ~205 Hz .. ~1,9 kHz */
        left = 4410;                              /* 100 ms */
        lit_pad = (int)a;
    }
}

static void tick(void)
{
    int want = left ? lit_pad : -1;
    if (want == shown)
        return;
    if (shown >= 0)
        host->led((unsigned int)shown, 0, 0, 0);
    if (want >= 0)
        host->led((unsigned int)want, 255, 255, 255);
    shown = want;
}

PGI_GUEST("bip", init, audio, event, tick);
