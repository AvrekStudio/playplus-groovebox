/* Enregistrement différé des réglages globaux (Projects/globalSettings).
 *
 * Problème (machine, 26/11/2026) : tourner vite le Master Volume fait craquer la sortie, même sans
 * aucun son, et seulement sur le Master. Trouvé au Play+ virtuel : à chaque tour de boucle,
 * fw_app_update (0x600c84f5) calcule une empreinte des 15 mots de réglages globaux (app + 0x4664 ..,
 * le Master est le 5e, app + 0x4674) ; si elle a changé, elle appelle fw_global_settings_save
 * (0x600c848d, bl en 0x600c8576), qui réécrit Projects/globalSettings sur la carte SD. Donc une
 * écriture SD par cran de Master (10 crans = 10 écritures), aucune pour Filter ou Tempo. Ces
 * écritures sont le suspect n° 1 des craquements (pic de courant de la carte, boucle occupée).
 *
 * Correction : le bl en 0x600c8576 appelle cfw_gs_save, qui ne fait que noter « à enregistrer » et
 * l'heure. L'officiel range quand même la nouvelle empreinte (il ne rappellera donc plus tant que rien
 * ne change). Notre enveloppe de fw_app_update (case 1 de la table de l'application 0x60078224, attendu
 * 0x600c84f5) fait l'enregistrement réel quand les réglages n'ont plus bougé depuis GS_DELAY_MS.
 * Résultat : une seule écriture à la fin d'un geste sur le Master au lieu d'une par cran.
 * Limite : si on éteint moins de GS_DELAY_MS après le dernier cran, ce dernier réglage n'est pas
 * enregistré (comme si on l'avait éteint juste avant de tourner).
 *
 * État : 12 o à cfw_ram + 0x28 (knob_touch.c : +0x00..0x0f, master_shift.c : +0x10..0x23). */
#include "cfw.h"
#include "target.h"

extern void fw_app_update(void *app);
extern int fw_global_settings_save(void *app);
extern volatile u32 fw_millis;
extern u8 cfw_ram[];
extern void br_tick(void);              /* browser.c : réglages, répétition de B0/B3 */

#define GS_MAGIC 0x47534446u            /* "FDSG" */
#define GS_DELAY_MS 1500u

struct gs_state {
    u32 magic;
    u32 pending;                        /* app + 1 si un enregistrement attend, sinon 0 */
    u32 since;                          /* fw_millis du dernier changement */
    u32 saves;                          /* enregistrements réels (pour les tests) */
    u32 deferred;                       /* demandes reçues de l'officiel (pour les tests) */
};

static volatile struct gs_state *gs(void)
{
    volatile struct gs_state *s = (volatile struct gs_state *)(cfw_ram + 0x28);
    if (s->magic != GS_MAGIC) {
        s->magic = GS_MAGIC;
        s->pending = 0;
        s->since = 0;
        s->saves = 0;
        s->deferred = 0;
    }
    return s;
}

/* Remplace l'appel de fw_global_settings_save dans fw_app_update. La valeur rendue n'est pas
 * utilisée par l'appelant (r0 est écrasé juste après, vérifié au désassemblage). */
int cfw_gs_save(void *app)
{
    volatile struct gs_state *s = gs();
    s->pending = (u32)app + 1;
    s->since = fw_millis;
    s->deferred++;
    return 0;
}

void cfw_gs_app_update(void *app)
{
    volatile struct gs_state *s;
    fw_app_update(app);                 /* indispensable : c'est la boucle de l'application */
    br_tick();
    s = gs();
    if (s->pending && fw_millis - s->since >= GS_DELAY_MS) {
        void *a = (void *)(s->pending - 1);
        s->pending = 0;
        s->saves++;
        fw_global_settings_save(a);
    }
}
