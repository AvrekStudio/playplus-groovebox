/* Mode invité : les LED des pads (jalon M3).
 *
 * LE PILOTE DE L'OFFICIEL (désassembleur + traces sur le Play+ virtuel, 25/11/2026)
 *   Objet pilote : fw_pad_leds = 0x20004d84 (gestionnaire des pads 0x20004d80 + 4, vtable 0x60076eb8).
 *     +0x00 u8  état : 0 repos, 1 attente avant envoi, 2 envoi DMA en cours
 *     +0x04 u8* trame encodée (0x1E30 = 7728 o, en SDRAM : 0x80002800 sur le virtuel)
 *     +0x08 u8  « trame modifiée » : la machine d'états (0x600afd74) l'envoie au plus toutes les 20 ms
 *     +0x14 + 4 x i : file d'attente (drapeau, r, g, b) utilisée quand une écriture tombe pendant un envoi.
 *   fw_led_encode(pilote, i, r, g, b) = 0x600af998 (5e argument sur la pile) : encode la LED i dans la trame
 *   (ou la met en file si état 2). Index i = rangée x 20 + colonne : vérifié, pad (3,10) -> 70,
 *   pad (7,19) -> 159. La trame a 161 LED de 48 o (i = 160 : inconnue, on n'y touche pas).
 *   Encodage d'une LED (lu dans 0x600af998) : 3 x 16 octets, vert puis rouge puis bleu ; dans chaque
 *   groupe, l'octet 1 + k porte le bit 7 - k de la couleur : 0x07 pour 1, 0x3f pour 0.
 *
 * LES DEUX CHEMINS PAR LESQUELS POLYEND ALLUME SES PADS (crochets, target.json)
 *   1. vtable du gestionnaire, slot 1 (mot 0x60076ebc -> 0x600ac580) : « couleur du pad »
 *      (gest, i, couleur, couleur2, mode). Si couleur2 == 0 et mode == 0, écriture immédiate dans le
 *      pilote (saut final vers 0x600af998) ; sinon il mémorise seulement (clignotement, etc.).
 *      -> cfw_pad_led_set : en mode invité on bloque l'écriture immédiate, on laisse passer le reste
 *         (l'état du gestionnaire reste juste, le clignotement repart à la sortie).
 *   2. mise à jour périodique des clignotements : BL 0x600af998 en 0x600aca2a.
 *      -> cfw_pad_led_blink : bloqué en mode invité.
 *
 * ENTRÉE / SORTIE : à l'entrée on copie la trame de Polyend dans un tampon du tas (alloué une fois) ;
 * à la sortie on la remet et on la marque modifiée (si un envoi est en cours, on attend la fin dans
 * guest_led_loop). Limite connue : ce que Polyend aurait changé pendant le mode invité (pads bloqués)
 * n'est pas rejoué ; il repeint de toute façon ses pads au prochain changement.
 *
 * L'INVITÉ (pour l'instant, mire de test) : pads tenus en orange, et une « tête de lecture » cyan qui
 * avance d'une colonne toutes les LED_STEP_MS. */
#include "cfw.h"
#include "target.h"
#include "guest.h"

extern char fw_pad_leds[];
extern void fw_led_encode(void *led, u32 i, u32 r, u32 g, u32 b);
extern void fw_pad_led_set(void *mgr, u32 i, const u8 *c, const u8 *c2, u32 mode);
extern void *fw_operator_new(u32 size);
extern volatile u32 fw_millis;

#define LED ((void *)fw_pad_leds)
#define LED_STATE 0x00
#define LED_FRAME 0x04
#define LED_DIRTY 0x08
#define LED_FRAME_SIZE 0x1E30
#define LED_STEP_MS 120

/* ---- crochets sur les écritures de Polyend ---------------------------------------------------- */

void cfw_pad_led_set(void *mgr, u32 i, const u8 *c, const u8 *c2, u32 mode)
{
    if (guest_drawing() && !c2 && !mode)
        return;                       /* écriture immédiate : les pads sont à l'invité */
    fw_pad_led_set(mgr, i, c, c2, mode);
}

void cfw_pad_led_blink(void *led, u32 i, u32 r, u32 g, u32 b)
{
    if (guest_drawing())
        return;
    fw_led_encode(led, i, r, g, b);
}

/* ---- entrée / sortie ----------------------------------------------------------------------------- */

static void copy_words(volatile u32 *dst, const volatile u32 *src, u32 n)
{
    while (n--)
        *dst++ = *src++;
}

void guest_led_enter(void)
{
    volatile struct guest_state *g = guest_state();
    u32 *frame = (u32 *)PTR(LED, LED_FRAME);
    if (!g->led_saved)
        g->led_saved = (u32)fw_operator_new(LED_FRAME_SIZE);
    if (g->led_saved && frame)
        copy_words((u32 *)g->led_saved, frame, LED_FRAME_SIZE / 4);
    g->led_restore = 0;
}

static void restore_now(volatile struct guest_state *g)
{
    u32 *frame = (u32 *)PTR(LED, LED_FRAME);
    if (g->led_saved && frame)
        copy_words(frame, (const u32 *)g->led_saved, LED_FRAME_SIZE / 4);
    U8(LED, LED_DIRTY) = 1;
    g->led_restore = 0;
}

void guest_led_exit(void)
{
    volatile struct guest_state *g = guest_state();
    g->led_restore = 1;
    if (U8(LED, LED_STATE) != 2)      /* pas d'envoi en cours : tout de suite */
        restore_now(g);
}

/* ---- dessin de l'invité, à chaque tour de boucle -------------------------------------------- */

void guest_led_loop(void)
{
    volatile struct guest_state *g = guest_state();
    u32 i;

    if (!guest_drawing()) {
        if (g->led_restore && U8(LED, LED_STATE) != 2)
            restore_now(g);
        return;
    }
    if (guest_pgi())
        return;                       /* un invité chargé pilote ses LED lui-même (pgi_host.led) */
    if (fw_millis - g->led_ms >= LED_STEP_MS) {
        g->led_ms = fw_millis;
        g->led_col = (g->led_col + 1) % 20;
        g->led_dirty = 1;
    }
    if (!g->led_dirty)
        return;
    g->led_dirty = 0;
    for (i = 0; i < 160; i++) {
        if ((g->pads[i >> 5] >> (i & 31)) & 1)
            fw_led_encode(LED, i, 255, 96, 0);       /* pad tenu : orange */
        else if (i % 20 == g->led_col)
            fw_led_encode(LED, i, 0, 40, 48);        /* tête de lecture : cyan doux */
        else
            fw_led_encode(LED, i, 0, 0, 0);
    }
}
