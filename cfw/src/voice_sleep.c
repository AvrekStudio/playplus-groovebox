/* Voice Sleep : ne pas calculer les voix de synthé au repos (analyse : docs/analyses/charge-audio.md,
 * 18 points de charge sur 23,7 en silence viennent des voix inactives, que l'officiel calcule toujours).
 *
 * Pour l'instant : le moteur VAP (Synth 1 du projet par défaut, ~3,9 % par voix). Trouvé le 25/11/2026 :
 *   - profil de l'interruption audio sur le Play+ virtuel : 0x6c00 (ITCM, copié depuis la flash) est
 *     appelée 3 fois par bloc, une par voix VAP, et l'oscillateur 0xfa94 qu'elle appelle fait 16 % du
 *     temps audio ;
 *   - 0x6c00 est atteinte par le relais 0x601315e0, slot 4 de la vtable des voix VAP (deux vtables :
 *     0x6008baac et 0x6008bae4, la seconde est celle des objets vus) ; arguments (objet, r1, r2, r3, s0,
 *     s1) : s1 = le tampon de sortie de la voix, 128 flottants, que 0x6c00 remet à zéro (0x6c84) puis
 *     remplit ;
 *   - slot 6 de la même vtable (0x60131558) : « la voix est-elle active ? » = octet objet + 0x114. Sur le
 *     virtuel, événement « note » (type 4 de l'aiguillage slot 2) : 0 -> 2 ; fin de note (type 5) :
 *     2 -> 4 ; fin du relâchement (~230 ms plus tard) : 4 -> 0. 0 = voix libre, au repos.
 * Règle : si l'état vaut 0, on ne calcule pas la voix et on met son tampon à zéro (ce qu'elle aurait
 * produit : rien). Une note passe l'état à 2 AVANT le calcul suivant : la voix se réveille d'elle-même.
 *
 * Interrupteur (Menu > Voice Sleep), ACTIF au démarrage depuis la v0.1 : validé sur la machine le
 * 25/11/2026 (48 % -> 32 % de charge au repos, rien de changé à l'oreille). État en cfw_ram + DIAG_SLEEP_OFFSET. Sous interruption audio : court, sans allocation. */
#include "cfw.h"
#include "target.h"
#include "diag.h"

extern void fw_vap_voice_render(void *obj, u32 a, u32 b, u32 c, u32 s0, u32 s1);
extern u8 cfw_ram[];

#define VOICE_STATE 0x114               /* octet : 0 libre, 2 note, 4 relâchement (voir plus haut) */
#define VOICE_FRAMES 128u

struct sleep_state {
    u32 magic;
    u32 on;
    u32 skipped;                        /* calculs de voix évités depuis l'activation */
    u32 rendered;                       /* calculs faits */
};

static volatile struct sleep_state *sl(void)
{
    volatile struct sleep_state *s = (volatile struct sleep_state *)(cfw_ram + DIAG_SLEEP_OFFSET);
    if (s->magic != DIAG_SLEEP_MAGIC) {
        s->magic = DIAG_SLEEP_MAGIC;
        s->on = 1;
        s->skipped = 0;
        s->rendered = 0;
    }
    return s;
}

int voice_sleep_toggle(void)
{
    volatile struct sleep_state *s = sl();
    s->on = !s->on;
    s->skipped = 0;
    s->rendered = 0;
    return (int)s->on;
}

void cfw_vap_voice_render(void *obj, u32 a, u32 b, u32 c, u32 s0, u32 s1)
{
    volatile struct sleep_state *s = sl();
    if (s->on && U8(obj, VOICE_STATE) == 0) {
        float *buf = (float *)s1;
        u32 i;
        for (i = 0; i < VOICE_FRAMES; i++)
            buf[i] = 0.0f;
        s->skipped++;
        return;
    }
    s->rendered++;
    fw_vap_voice_render(obj, a, b, c, s0, s1);
}
