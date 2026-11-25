/* Réglage « Knob Touch » (Menu > Settings) : le toucher des potards du Play+ peut être désactivé.
 *
 * Demandé par le propriétaire le 25/11/2026, pour la période où il apprend la machine (firmware minimal :
 * l'officiel 1.3.0 + ce réglage seulement, cible playplus-1.3.0-base). Code à nous ; la même option existe
 * dans FreePlay (« Knob Touch : Normal / Turn Only ») : on en a repris les adresses et le comportement
 * décrit, pas le code.
 *
 * COMPORTEMENT DE L'OFFICIEL : les potards sont tactiles. Le slot 3 de la vtable du listener des entrées
 * (0x6007b288 -> 0x600d9d6c) reçoit le toucher (potard, état 0 touché / 1 lâché / 2 tenu) : le Play+
 * affiche le paramètre de ce potard et en fait le paramètre courant ; le double toucher donne accès au
 * paramètre secondaire (manuel). Tourner un potard (slot 2) choisit aussi son paramètre.
 * AVEC « Knob Touch : Off » : les événements de toucher sont avalés ; tourner marche comme avant. Le
 * double toucher (paramètre secondaire) n'est plus disponible.
 *
 * MENU : le pointeur du constructeur du sous-menu Settings est dans le pool littéral du constructeur du
 * menu racine (0x600e9f50 -> 0x600eac40). cfw_settings_build appelle l'original puis ajoute une ligne
 * d'option de l'officiel (0x600e84a0, la même que « Grid Brightness ») qui écrit directement la valeur.
 *
 * SAUVEGARDE : /Projects/avrekKnobTouch (1 octet : 0 = On, 1 = Off), lu une fois au premier besoin (boucle
 * principale : construction du menu ou premier toucher), écrit à chaque changement. Pas de fichier = On
 * (comme l'officiel). État en cfw_ram (DTCM, non initialisée au démarrage : protégée par une marque). */
#include "cfw.h"
#include "target.h"

extern void fw_in_track(void *l, u32 a, u32 b, u32 c);
extern void fw_menu_build_settings(void *ctx, void *menu);
extern void *fw_menu_option_init(void *item, void *menu, const char *label, u32 *val, u32 max,
                                 const char *const *labels, std_function *on_change);
extern int fw_fn_manager_word(void *dst, const void *src, int op);
extern void *fw_operator_new(u32 size);
extern int fw_file_open(void *f, const char *path, u32 mode);
extern int fw_file_read(void *f, void *buf, u32 n);
extern int fw_file_write(void *f, const void *buf, u32 n);
extern void fw_file_close(void *f);
extern u8 cfw_ram[];

#define KT_MAGIC 0x4B54434Bu            /* "KCTK" */
#define KT_PATH "/Projects/avrekKnobTouch"
#define OPTION_ITEM_SIZE 0x60           /* ligne d'option de l'officiel (taille de l'objet, symbols.ld) */
#define FILE_OBJ 0x70
#define FA_READ 0x01
#define FA_WRITE_NEW 0x0a

struct kt_state {
    u32 magic;
    u32 loaded;
    u32 off;                            /* valeur de la ligne d'option : 0 = On, 1 = Off */
    u32 dropped;                        /* touchers avalés (pour les tests) */
};

static volatile struct kt_state *kt(void)
{
    volatile struct kt_state *s = (volatile struct kt_state *)cfw_ram;
    if (s->magic != KT_MAGIC) {
        s->magic = KT_MAGIC;
        s->loaded = 0;
        s->off = 0;
        s->dropped = 0;
    }
    return s;
}

static void zero(u8 *p, u32 n)
{
    while (n--)
        *(volatile u8 *)p++ = 0;
}

static void load(volatile struct kt_state *s)
{
    u8 f[FILE_OBJ] __attribute__((aligned(4)));
    u8 v = 0;
    s->loaded = 1;
    zero(f, FILE_OBJ);
    if (!fw_file_open(f, KT_PATH, FA_READ))
        return;
    if (fw_file_read(f, &v, 1) == 1)
        s->off = v ? 1u : 0u;
    fw_file_close(f);
}

static void save(void *const *storage)
{
    u8 f[FILE_OBJ] __attribute__((aligned(4)));
    u8 v = (u8)kt()->off;
    (void)storage;
    zero(f, FILE_OBJ);
    if (!fw_file_open(f, KT_PATH, FA_WRITE_NEW))
        return;
    fw_file_write(f, &v, 1);
    fw_file_close(f);
}

/* listener des entrées, slot 3 : le toucher d'un potard */
void cfw_in_track(void *l, u32 a, u32 b, u32 c)
{
    volatile struct kt_state *s = kt();
    if (!s->loaded)
        load(s);
    if (s->off) {
        s->dropped++;
        return;
    }
    fw_in_track(l, a, b, c);
}

static const char *const kt_labels[2] = {"On", "Off"};

/* Menu > Settings : les lignes de l'officiel, puis « Knob Touch  On / Off » */
void cfw_settings_build(void *ctx, void *menu)
{
    volatile struct kt_state *s = kt();
    std_function fn = {ctx, 0, fw_fn_manager_word, save};
    if (!s->loaded)
        load(s);
    fw_menu_build_settings(ctx, menu);
    fw_menu_option_init(fw_operator_new(OPTION_ITEM_SIZE), menu, "Knob Touch", (u32 *)&s->off, 1, kt_labels, &fn);
    fn.manager(&fn, &fn, 3);            /* 3 = détruire notre copie locale (la ligne a la sienne) */
}
