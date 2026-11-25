/* Boucle principale : notre point d'entrée à chaque passage de la boucle du firmware.
 *
 * Crochet (target.json, « app vtable slot 1 ») : l'objet application de l'officiel a une vtable à
 * 0x60078224. Son slot 1 (mot 0x60078228) pointe sur fw_app_update (0x600c84f5), appelée une fois par
 * tour de boucle principale (hors interruption : on peut y prendre son temps, raisonnablement). Le build
 * remplace ce mot par cfw_app_update. On DOIT appeler fw_app_update nous-mêmes, sinon la machine
 * ne fait plus rien. Le vérificateur a confirmé que cette vtable est bien chargée par du code de
 * l'officiel (0x600c82b4 et 0x600c948e).
 *
 * fw_millis (0x200064c8) : compteur de millisecondes de l'officiel (voir symbols.ld).
 * cfw_ram (0x20002700) : 256 octets de RAM inutilisés par l'officiel, notre seul état modifiable
 * pour l'instant. Sa carte d'occupation est tenue dans diag.h.
 * On appelle d'abord la fonction d'origine, puis notre travail. Pour l'instant : un compteur de passes
 * et la durée de la dernière passe, en RAM de travail (cfw_ram), lisibles par les tests et, plus tard,
 * par une page de diagnostic. C'est le socle des futures fonctions (mesure de charge, écran, macros). */
#include "cfw.h"
#include "target.h"
#include "diag.h"
#include "guest.h"

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
    volatile struct loop_state *st = (volatile struct loop_state *)(cfw_ram + DIAG_LOOP_OFFSET);
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
    audio_load_install();         /* (re)met notre chronomètre sur le rendu audio, voir audio_load.c */
    guest_loop();                 /* mode invité : fermeture du menu à l'activation, voir guest.c */
    if (st->passes > 100 && now - st->last_ms > st->max_gap)
        st->max_gap = now - st->last_ms;
    st->last_ms = now;
}
