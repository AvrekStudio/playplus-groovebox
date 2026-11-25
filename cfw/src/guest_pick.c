/* Mode invité : sélecteur d'invités (plusieurs fichiers PGI sur la carte, chargés à la volée).
 *
 * À l'entrée du mode invité, guest_pick_enter() liste les fichiers « *.pgi » du dossier PGI_DIR
 * (/Guests) par la FatFs de l'officiel (fw_f_opendir / fw_f_readdir / fw_f_closedir ; tailles DIR 0x60
 * et FILINFO 0x120, attributs en +0xc, nom long en +0x1a : mêmes valeurs que les outils FreePlay).
 * La liste (au plus PICK_MAX noms de PICK_NAME - 1 caractères, plus longs ignorés) est sur le tas,
 * allouée une fois ; son adresse est dans guest_state()->pick.
 *
 * Commandes du sélecteur :
 *   encodeur : déplace le curseur ; clic de l'encodeur (bouton 6) ou touches 0 / 3 : charge l'invité
 *   choisi ; pad i (i < nombre d'invités) : charge directement le i-ème.
 *   Depuis un invité lancé : Shift + clic de l'encodeur revient ici (guest.c). Shift + Back : sortie.
 * LED : un pad par invité (bleu), le curseur en blanc.
 * Pendant le sélecteur, le son est coupé (audio_load.c). */
#include "cfw.h"
#include "target.h"
#include "guest.h"
#include "pgi.h"

extern int fw_f_opendir(void *dir, const char *path);
extern int fw_f_readdir(void *dir, void *fno);
extern int fw_f_closedir(void *dir);
extern void *fw_operator_new(u32 size);
extern char fw_pad_leds[];
extern void fw_led_encode(void *led, u32 i, u32 r, u32 g, u32 b);

#define DIR_SIZE 0x60
#define FNO_SIZE 0x120
#define FNO_ATTR 0x0c
#define FNO_NAME 0x1a
#define AM_HID 0x02
#define AM_DIR 0x10
#define PICK_MAX 32
#define PICK_NAME 28
#define BTN_ENTER_A 0
#define BTN_ENTER_B 3

struct pick {
    u32 n;
    u32 cursor;
    char names[PICK_MAX][PICK_NAME];   /* noms de fichiers, extension comprise */
};

static struct pick *pk(void)
{
    volatile struct guest_state *g = guest_state();
    if (!g->pick)
        g->pick = (u32)fw_operator_new(sizeof(struct pick));
    return (struct pick *)g->pick;
}

static char lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

/* « xxx.pgi » (casse ignorée), et pas trop long pour la liste */
static int is_pgi(const char *nm)
{
    u32 n = 0;
    while (nm[n])
        n++;
    return n > 4 && n < PICK_NAME && nm[n - 4] == '.' && lower(nm[n - 3]) == 'p' && lower(nm[n - 2]) == 'g' &&
           lower(nm[n - 1]) == 'i';
}

int guest_pick_enter(void)
{
    u8 dir[DIR_SIZE] __attribute__((aligned(4)));
    u8 fno[FNO_SIZE] __attribute__((aligned(4)));
    struct pick *p = pk();
    u32 i;
    if (!p)
        return 0;
    p->n = 0;
    p->cursor = 0;
    if (fw_f_opendir(dir, PGI_DIR) == 0) {
        while (p->n < PICK_MAX && fw_f_readdir(dir, fno) == 0) {
            const char *nm = (const char *)fno + FNO_NAME;
            if (!nm[0])
                break;
            if ((fno[FNO_ATTR] & (AM_DIR | AM_HID)) || !is_pgi(nm))
                continue;
            for (i = 0; nm[i]; i++)
                p->names[p->n][i] = nm[i];
            p->names[p->n][i] = 0;
            p->n++;
        }
        fw_f_closedir(dir);
    }
    guest_state()->pick_err = 0;
    guest_state()->led_dirty = 1;
    return p->n > 0;
}

u32 guest_pick_count(void)
{
    struct pick *p = (struct pick *)guest_state()->pick;
    return p ? p->n : 0;
}

u32 guest_pick_cursor(void)
{
    struct pick *p = (struct pick *)guest_state()->pick;
    return p ? p->cursor : 0;
}

const char *guest_pick_name(u32 i)
{
    struct pick *p = (struct pick *)guest_state()->pick;
    return p && i < p->n ? p->names[i] : 0;
}

/* charge le i-ème invité : « /Guests/<nom> » */
static void load(u32 i)
{
    struct pick *p = pk();
    volatile struct guest_state *g = guest_state();
    char path[sizeof(PGI_DIR) + PICK_NAME + 1];
    u32 k = 0, j;
    if (!p || i >= p->n)
        return;
    for (j = 0; PGI_DIR[j]; j++)
        path[k++] = PGI_DIR[j];
    path[k++] = '/';
    for (j = 0; p->names[i][j]; j++)
        path[k++] = p->names[i][j];
    path[k] = 0;
    p->cursor = i;
    if (guest_load(path)) {
        g->picking = 0;
        g->pick_err = 0;
        for (j = 0; j < 160; j++)        /* l'invité part d'un plateau éteint */
            fw_led_encode(fw_pad_leds, j, 0, 0, 0);
    } else {
        g->pick_err = 1;
    }
    g->led_dirty = 1;
}

void guest_pick_event(u32 kind, u32 a, u32 b)
{
    struct pick *p = pk();
    if (!p || !p->n)
        return;
    if (kind == GEV_ENCODER) {
        s32 c = (s32)p->cursor + (s32)b;
        if (c < 0)
            c = 0;
        if (c >= (s32)p->n)
            c = (s32)p->n - 1;
        p->cursor = (u32)c;
        guest_state()->led_dirty = 1;
    } else if (kind == GEV_BUTTON && b == 0 && (a == BTN_ENCODER || a == BTN_ENTER_A || a == BTN_ENTER_B)) {
        load(p->cursor);
    } else if (kind == GEV_PAD && b == 0 && a < p->n) {
        load(a);
    }
}

void guest_pick_leds(void)
{
    struct pick *p = (struct pick *)guest_state()->pick;
    u32 i;
    for (i = 0; i < 160; i++) {
        if (p && i < p->n && i == p->cursor)
            fw_led_encode(fw_pad_leds, i, 255, 255, 255);
        else if (p && i < p->n)
            fw_led_encode(fw_pad_leds, i, 0, 60, 255);
        else
            fw_led_encode(fw_pad_leds, i, 0, 0, 0);
    }
}
