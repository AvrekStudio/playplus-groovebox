/* Boucle principale : notre point d'entrée à chaque passage de la boucle du firmware.
 *
 * Hook (target.json) : slot 1 de la vtable de l'application (0x60078224), appelé une fois par passe.
 * On appelle d'abord la fonction d'origine, puis notre travail. Pour l'instant : un compteur de passes
 * et la durée de la dernière passe, en RAM de travail (cfw_ram), lisibles par les tests et, plus tard,
 * par une page de diagnostic. C'est le socle des futures fonctions (mesure de charge, écran, macros). */
#include "cfw.h"
#include "target.h"

extern void fw_app_update(void *app);
extern volatile u32 fw_millis;
extern u8 cfw_ram[];

#define LOOP_MAGIC 0x41564B31u /* "AVK1" */

struct loop_state {
    u32 magic;
    u32 passes;    /* passes de boucle depuis le démarrage */
    u32 last_ms;   /* fw_millis à la passe précédente */
    u32 max_gap;   /* plus long écart entre deux passes, en ms (après la mise en route) */
};

void cfw_app_update(void *app)
{
    volatile struct loop_state *st = (volatile struct loop_state *)cfw_ram;
    u32 now;

    fw_app_update(app);

    now = fw_millis;
    if (st->magic != LOOP_MAGIC) {
        st->magic = LOOP_MAGIC;
        st->passes = 0;
        st->max_gap = 0;
        st->last_ms = now;
    }
    st->passes++;
    if (st->passes > 100 && now - st->last_ms > st->max_gap)
        st->max_gap = now - st->last_ms;
    st->last_ms = now;
}
