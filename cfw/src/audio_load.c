/* Mesure de la charge audio : combien de cycles CPU coûte le calcul de chaque bloc audio.
 *
 * Comment l'officiel calcule le son (vérifié sur le Play+ virtuel, voir docs/JOURNAL.md) :
 *   l'interruption de fin de DMA audio (routine en ITCM à 0x00013a18, IRQ 0) bascule le double
 *   tampon puis appelle le « callback de rendu » via un pointeur en RAM, fw_audio_render_cb
 *   (0x2000301c ; valeur après démarrage : 0x600ad379). Ce callback calcule les voix et fait avancer
 *   le séquenceur pour un bloc de 128 échantillons. À 44,1 kHz, un bloc dure 128 / 44100 s = 2,902 ms,
 *   soit 1 741 497 cycles à 600 MHz : c'est le budget, 100 % de charge.
 *
 * Ce qu'on fait : à chaque tour de boucle principale, audio_load_install() vérifie que ce pointeur
 * désigne notre cfw_audio_render ; sinon il mémorise la valeur actuelle comme « original » et met la
 * nôtre. Ce n'est pas un crochet de target.json (le pointeur est en RAM, écrit au démarrage par
 * l'officiel), d'où la réinstallation permanente : si l'officiel le réécrit (chargement de projet...),
 * on le reprend au tour suivant. Ordre des écritures : l'original d'abord, le pointeur ensuite (une
 * seule écriture 32 bits, atomique), donc l'interruption ne voit jamais un état incohérent.
 *
 * cfw_audio_render tourne SOUS INTERRUPTION : quelques lectures du compteur de cycles et quelques
 * additions, rien d'autre (ni division, ni appel au firmware autre que le rendu d'origine).
 *
 * Le compteur de cycles est le DWT CYCCNT du Cortex-M7 (0xE0001004). L'officiel l'active déjà au
 * démarrage (DEMCR.TRCENA et DWT_CTRL.CYCCNTENA à 1, constaté dans le Play+ virtuel). Sur le Play+
 * virtuel, ce compteur n'est qu'un ordre de grandeur (voir exact_count dans cfw/vdev/vplay.py).
 *
 * Résultats dans cfw_ram + 0x10 (struct audio_state), lus par la page de diagnostic et les tests. */
#include "cfw.h"
#include "target.h"
#include "diag.h"
#include "guest.h"
#include "pgi.h"

typedef u32 (*render_fn)(u32, u32, u32, u32);

extern render_fn volatile fw_audio_render_cb;
extern u8 cfw_ram[];

#define DWT_CYCCNT (*(volatile u32 *)0xE0001004u)

static volatile struct audio_state *audio_state(void)
{
    return (volatile struct audio_state *)(cfw_ram + DIAG_AUDIO_OFFSET);
}

/* Remplace le callback de rendu : chronomètre l'original. Mêmes arguments, même valeur de retour. */
u32 cfw_audio_render(u32 a, u32 b, u32 c, u32 d)
{
    volatile struct audio_state *st = audio_state();
    u32 t0 = DWT_CYCCNT, dt, r;

    if (guest_active()) {             /* mode invité : notre rendu à la place de celui de Polyend */
        const struct pgi_header *pg = guest_pgi();
        if (pg && pg->audio)
            pg->audio((short *)a, (short *)b, 128);   /* invité chargé depuis la carte SD */
        else
            guest_render((s16 *)a, (s16 *)b);
        r = 0;
    } else {
        r = ((render_fn)st->orig)(a, b, c, d);
    }
    dt = DWT_CYCCNT - t0;

    st->blocks++;
    st->last = dt;
    if (dt > st->win_peak)
        st->win_peak = dt;
    if (dt > st->peak_all)
        st->peak_all = dt;
    st->win_sum += dt;
    if (++st->win_n >= DIAG_WINDOW_BLOCKS) {      /* environ 1 s : on publie la fenêtre */
        st->avg_done = st->win_sum / DIAG_WINDOW_BLOCKS;   /* une division par seconde : négligeable */
        st->peak_done = st->win_peak;
        st->win_sum = 0;
        st->win_peak = 0;
        st->win_n = 0;
        st->windows++;
    }
    return r;
}

/* Appelée à chaque tour de boucle principale (loop.c), hors interruption. */
void audio_load_install(void)
{
    volatile struct audio_state *st = audio_state();
    render_fn cur = fw_audio_render_cb;

    if (st->magic != DIAG_AUDIO_MAGIC) {
        u32 i;
        for (i = 0; i < sizeof(struct audio_state) / 4; i++)
            ((volatile u32 *)st)[i] = 0;
        st->magic = DIAG_AUDIO_MAGIC;
    }
    if (cur == 0 || cur == (render_fn)cfw_audio_render)
        return;                       /* pas encore initialisé par l'officiel, ou déjà en place */
    st->orig = (u32)cur;              /* 1) l'original d'abord ... */
    fw_audio_render_cb = (render_fn)cfw_audio_render;   /* 2) ... puis le pointeur */
    st->installs++;
}
