/* Nos entrées dans le menu principal.
 *
 * Crochet (target.json, « root Menu builder pointer ») : la fonction qui ouvre le menu principal
 * (FUN_600c97c4) charge l'adresse du constructeur du menu racine depuis son pool de littéraux, au mot
 * 0x600c9820, qui vaut fw_menu_build_root (0x600e9e59) dans l'officiel. Le build y met
 * cfw_menu_build_root : on construit d'abord le menu d'origine (File, Samples, Synths, Scales, MIDI,
 * Settings), puis on ajoute nos lignes à la fin.
 *
 * Une ligne « action » = un objet de ACTION_SIZE octets (alloué avec l'operator new de l'officiel),
 * initialisé par fw_menu_action_init avec un std::function (voir cfw.h). Le menu garde l'objet ;
 * on détruit seulement notre copie locale du std::function, comme le font les constructeurs d'origine.
 * Cette recette vient de FreePlay (slice.c, cfw_add_action), vérifiée sur le Play+ virtuel. */
#include "cfw.h"
#include "target.h"
#include "diag.h"
#include "fmt.h"
#include "guest.h"

extern void *fw_operator_new(u32 size);
extern void *fw_menu_action_init(void *item, void *menu, const char *label, std_function *fn, u32 flag);
extern void fw_menu_build_root(void *ctx, void *menu);
extern int fw_fn_manager_word(void *dst, const void *src, int op);
extern void fw_toast_show(void *toast, const char *msg);
extern u8 cfw_ram[];

static void add_action(void *ctx, void *menu, const char *label, void (*inv)(void *const *))
{
    std_function fn = {ctx, 0, fw_fn_manager_word, inv};
    void *item = fw_operator_new(ACTION_SIZE);
    fw_menu_action_init(item, menu, label, &fn, 0);
    fn.manager(&fn, &fn, 3); /* 3 = détruire notre copie locale */
}

/* « Diagnostic » : un message (toast) avec la charge audio de la dernière seconde.
 * Ex. « Audio 24.7% pic 31.2% ». Première version visible de l'étape 1 ; une vraie page suivra.
 * Charge = cycles mesurés / budget d'un bloc (DIAG_BLOCK_BUDGET), en dixièmes de pour cent. */
static void show_diagnostic(void *const *storage)
{
    void *ctx = storage[0];
    const volatile struct audio_state *a = (const volatile struct audio_state *)(cfw_ram + DIAG_AUDIO_OFFSET);
    char msg[48], *p = msg;

    if (a->magic != DIAG_AUDIO_MAGIC || a->windows == 0) {
        p = fmt_str(p, "Audio: mesure en cours");
    } else {
        /* Charge en dixièmes de % = cycles / (budget / 1000). DIAG_BLOCK_BUDGET / 1000 = 1741 (au lieu
         * de 1741,497 : 0,03 % d'écart). Tout en u32 : pas de division 64 bits (pas de bibliothèque
         * d'exécution sur la machine). */
        u32 per_tenth = DIAG_BLOCK_BUDGET / 1000u;
        u32 avg_t = a->avg_done / per_tenth;
        u32 peak_t = a->peak_done / per_tenth;
        p = fmt_str(p, "Audio ");
        p = fmt_tenths(p, avg_t);
        p = fmt_str(p, "% pic ");
        p = fmt_tenths(p, peak_t);
        *p++ = '%';
    }
    *p = 0;
    fw_toast_show((char *)PTR(ctx, MENU_CTX_UI) + UI_TOAST, msg);
}

/* « Mode invite » : bascule en mode invité (guest.c). Sortie : Shift + Back.
 * (Pas d'accent : la police du firmware n'a peut-être pas les caractères accentués.) */
static void enter_guest(void *const *storage)
{
    (void)storage;
    guest_enter(0);
}

/* « Explorateur » : le mode invité sans invité, qui affiche chaque commande touchée (son numéro, son
 * état) : pour établir la table de correspondance des commandes sur la machine. */
static void enter_explorer(void *const *storage)
{
    (void)storage;
    guest_enter(1);
}

/* « Voice Sleep » : ne plus calculer les voix de synthé VAP au repos (voice_sleep.c) ; message : l'état
 * choisi. À comparer à l'oreille et avec Diagnostic. */
int voice_sleep_toggle(void);
static void toggle_sleep(void *const *storage)
{
    void *ctx = storage[0];
    fw_toast_show((char *)PTR(ctx, MENU_CTX_UI) + UI_TOAST, voice_sleep_toggle() ? "Voice Sleep actif" : "Voice Sleep inactif");
}

/* « Auto-test » : vérifie sur la machine ce que le Play+ virtuel ne sait pas émuler (exécution en OCRAM
 * et en SDRAM, protection mémoire, mémoire des samples), puis affiche l'explorateur avec les résultats. */
static void run_selftest(void *const *storage)
{
    (void)storage;
    guest_selftest();
}

void cfw_menu_build_root(void *ctx, void *menu)
{
    fw_menu_build_root(ctx, menu);
    add_action(ctx, menu, "Diagnostic", show_diagnostic);
    add_action(ctx, menu, "Mode invite", enter_guest);
    add_action(ctx, menu, "Explorateur", enter_explorer);
    add_action(ctx, menu, "Auto-test", run_selftest);
    add_action(ctx, menu, "Voice Sleep", toggle_sleep);
}
