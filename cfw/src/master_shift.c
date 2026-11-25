/* Master Volume sur Shift + encodeur, et potard 1 (index 0) libéré.
 *
 * Demande du propriétaire (25/11/2026) : dans l'officiel, le premier potard (rangée 1, à gauche,
 * index 0) est « Master Volume / Limiter ». Il ne veut plus de ça : le potard 0 ne fait plus rien
 * (ni rotation ni toucher), et le volume général se règle en tenant Shift et en tournant l'encodeur
 * cranté sous l'écran.
 *
 * Comment : trois entrées de la table du récepteur d'entrées 0x6007b27c (voir symbols.ld) passent par
 * nous ; la quatrième (toucher, [3]) est dans knob_touch.c, qui appelle ms_knob_blocked().
 *   [1] bouton  0x6007b280 (attendu fw_in_button  0x600d9d95) : on suit l'état de Shift (bouton 10 ;
 *       0 appui, 2 tenu, 1 relâché), puis l'officiel reçoit l'évènement tel quel.
 *   [2] potard  0x6007b284 (attendu fw_in_knob    0x600d9d3d) : a = potard, b = crans signés.
 *       Potard 0 avalé, les autres passent.
 *   [4] encodeur 0x6007b28c (attendu fw_in_encoder 0x600d9e5d) : b = crans signés. Shift tenu :
 *       on les donne à l'officiel comme des crans du potard 0. C'est donc le code d'origine qui règle
 *       le volume, avec sa bulle « Master Volume  -x.x dB » (Shift lui donne aussi son pas plus gros).
 *       Sans Shift, l'encodeur fait ce qu'il faisait.
 * Piège vu au test (26/11) : l'encodeur seul règle le paramètre courant de l'officiel (contrôleur
 *   = app 0x20018c18 + 0x38, index du paramètre courant à +0x240c, note FreePlay vérifiée au test).
 *   Nos crans « potard 0 » font de Master Volume le paramètre courant ; sans rien faire, l'encodeur
 *   seul réglerait ensuite le volume. On retient donc le paramètre d'avant au premier cran et on le
 *   remet quand Shift est relâché.
 * Conséquence : Shift + encodeur ne fait plus son ancien rôle (dans l'officiel, pas plus gros dans
 * les listes).
 *
 * Volume observé dans le Play+ virtuel (26/11) : mot à 0x2001d28c (objet application 0x20018c18
 * + 0x4674), 104 au départ, -1 par cran du potard 0, affiché -1.5 dB à 101. Sert aux tests seulement.
 *
 * État : 20 o à cfw_ram + 0x10 (knob_touch.c occupe les 16 premiers). La RAM n'est pas remise à
 * zéro au redémarrage, d'où le nombre magique. */
#include "cfw.h"
#include "target.h"

extern void fw_in_button(void *l, u32 a, u32 b, u32 c);
extern void fw_in_knob(void *l, u32 a, u32 b, u32 c);
extern void fw_in_encoder(void *l, u32 a, u32 b, u32 c);
extern u8 cfw_ram[];

#define MS_MAGIC 0x4D53484Du            /* "MHSM" */
#define BTN_SHIFT 10                    /* table des boutons relevée sur la machine (25/11) */
#define FREED_KNOB 0                    /* le potard libéré */
extern u32 fw_cur_param;                /* symbols.ld : 0x2001b05c, voir le piège ci-dessus */
#define CUR_PARAM ((volatile u32 *)&fw_cur_param)

struct ms_state {
    u32 magic;
    u32 shift;                          /* Shift enfoncé */
    u32 dropped;                        /* évènements du potard 0 avalés (pour les tests) */
    u32 routed;                         /* crans d'encodeur envoyés au volume (pour les tests) */
    u32 saved;                          /* paramètre courant d'avant + 1 ; 0 = rien de retenu */
};

static volatile struct ms_state *ms(void)
{
    volatile struct ms_state *s = (volatile struct ms_state *)(cfw_ram + 0x10);
    if (s->magic != MS_MAGIC) {
        s->magic = MS_MAGIC;
        s->shift = 0;
        s->dropped = 0;
        s->routed = 0;
        s->saved = 0;
    }
    return s;
}

/* knob_touch.c : le toucher du potard libéré est avalé aussi, sinon il afficherait Master Volume. */
int ms_knob_blocked(u32 k)
{
    if (k != FREED_KNOB)
        return 0;
    ms()->dropped++;
    return 1;
}

void cfw_in_button(void *l, u32 a, u32 b, u32 c)
{
    volatile struct ms_state *s = ms();
    if (a == BTN_SHIFT) {
        s->shift = (b != 1);
        if (b == 1 && s->saved) {
            *CUR_PARAM = s->saved - 1;  /* Shift relâché : l'encodeur retrouve son paramètre */
            s->saved = 0;
        }
    }
    fw_in_button(l, a, b, c);
}

void cfw_in_knob(void *l, u32 a, u32 b, u32 c)
{
    if (ms_knob_blocked(a))
        return;
    fw_in_knob(l, a, b, c);
}

void cfw_in_encoder(void *l, u32 a, u32 b, u32 c)
{
    volatile struct ms_state *s = ms();
    if (s->shift) {
        s->routed++;
        if (!s->saved)
            s->saved = *CUR_PARAM + 1;
        fw_in_knob(l, FREED_KNOB, b, c);
        return;
    }
    fw_in_encoder(l, a, b, c);
}
