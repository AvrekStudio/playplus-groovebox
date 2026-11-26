/* Navigateur de samples (Menu > Samples > Load Files or Folders) à notre sauce.
 *
 * Demande du propriétaire (26/11/2026), sur cet écran seulement :
 *   - B0 = flèche haut, B3 = flèche bas (comme un clavier), avec répétition si on tient ;
 *   - Shift + Back = auto-preview On/Off ; Shift tenu, l'étiquette au-dessus de Back montre l'état ;
 *     auto-preview On : chaque déplacement sur un fichier lance sa pré-écoute ;
 *   - pré-écoute à 50 % par défaut, réglable au potard 1 (index 0, libéré par master_shift.c),
 *     avec une jauge verticale à gauche de l'écran.
 *
 * L'OFFICIEL (relevé au désassembleur et au Play+ virtuel, 26/11/2026 ; adresses dans symbols.ld)
 *   - Gestionnaire d'interface ui = [fw_input_listener + 8] ; écran courant [ui + 0x300] ; l'écran
 *     navigateur est [ui + 0x08] (table 0x6008608c). Le répartiteur de boutons (fw_in_button) donne
 *     d'abord le bouton à la case 9 de l'écran (écran, bouton, état, shift).
 *   - Écran navigateur : +0x38 lecteur de pré-écoute, +0x50 mode (2 = liste de fichiers, celui de
 *     « Load Files or Folders »), +0x5c ligne sélectionnée, +0x68 tableau des noms (char *), +0x6c
 *     dossier courant (char *), +0x294 sous-mode (0 = liste ; sinon clavier, renommage...).
 *     Un nom qui commence par '/' est un dossier (« /.. » compris).
 *   - « Entrée » (B0, B3 ou clic de l'encodeur, sur appui) sur un fichier : chemin « dossier/nom »
 *     puis fw_preview_play(lecteur, chemin). C'est tout ce que fait l'officiel ; on fait pareil.
 *   - La pré-écoute est ajoutée au bus de mixage par fw_mix_add16 SANS gain (pleine échelle), en
 *     0x6010f51e et 0x6010f536. On remplace ces deux appels par cfw_preview_mix (avec gain).
 *   - Étiquettes des touches sous l'écran : case 11 de l'écran (fw_browser_label), interrogée à
 *     chaque mise à jour ; la barre se redessine d'elle-même quand le texte change (vérifié au test).
 *   - Image : recomposée seulement quand quelque chose change (voir guest_screen.c). On ajoute la
 *     jauge juste avant le DISPLAY de la composition (bl en 0x600ab2f8) et, quand le volume change,
 *     on demande une recomposition (état 2 de l'automate) depuis cfw_br_display_update.
 *
 * Réglages enregistrés dans /Projects/avrekBrowser (2 octets : auto-preview 0/1, volume 0..100),
 * 1,5 s après le dernier changement, depuis la boucle principale (jamais sous interruption).
 * État : cfw_ram + 0x40 .. 0x7f (knob_touch.c 0x00, master_shift.c 0x10, settings_defer.c 0x28). */
#include "cfw.h"
#include "target.h"

extern void fw_in_button(void *l, u32 a, u32 b, u32 c);
extern void fw_in_encoder(void *l, u32 a, u32 b, u32 c);
extern void fw_preview_play(void *player, const char *path);
extern void fw_mix_add16(s32 *bus, const u32 *src);
extern const char *fw_browser_label(void *screen, u32 key);
extern void fw_eve_display(void);
extern void fw_eve_word(u32 w);
extern void fw_eve_begin(void);
extern void fw_eve_end(void);
extern void fw_display_update(void *dm);
extern int fw_file_open(void *f, const char *path, u32 mode);
extern int fw_file_read(void *f, void *buf, u32 n);
extern int fw_file_write(void *f, const void *buf, u32 n);
extern void fw_file_close(void *f);
extern volatile u16 fw_eve_wp;
extern volatile u8 fw_shift_held;
extern volatile u32 fw_millis;
extern u8 fw_input_listener[];
extern u8 cfw_ram[];

#define BR_MAGIC 0x4252574Fu            /* "OWRB" */
#define BR_PATH "/Projects/avrekBrowser"
#define FILE_OBJ 0x70
#define FA_READ 0x01
#define FA_WRITE_NEW 0x0a
#define SAVE_DELAY_MS 1500u
#define REPEAT_DELAY_MS 400u            /* tenir B0/B3 : première répétition */
#define REPEAT_MS 90u                   /* puis une ligne toutes les 90 ms */
#define VOL_DEFAULT 50u                 /* % : « diviser par 2 le volume de la preview » */

#define BTN_UP 0                        /* B0, en haut à gauche du bloc 3 x 3 */
#define BTN_DOWN 3                      /* B3, en dessous */
#define BTN_BACK 7
#define UI_BROWSER 0x08
#define UI_CUR 0x300
#define BRW_PLAYER 0x38
#define BRW_MODE 0x50
#define BRW_SEL 0x5c
#define BRW_NAMES 0x68
#define BRW_DIR 0x6c
#define BRW_SUB 0x294
#define MODE_FILES 2
#define DM_STATE 0x14                   /* automate d'affichage (guest_screen.c) */
#define DM_READY 0x34


struct br_state {
    u32 magic;
    u32 loaded;
    u32 autoprev;                       /* 0 / 1 */
    u32 vol;                            /* 0..100 % */
    u32 gain;                           /* vol en Q15, lu sous interruption */
    u32 dirty;                          /* 1 : à enregistrer */
    u32 since;                          /* fw_millis du dernier changement */
    u32 held;                           /* bouton B0/B3 tenu + 1, 0 sinon */
    u32 held_since;
    u32 last_repeat;
    u32 redraw;                         /* 1 : recomposer l'image (jauge) */
    u32 eat_back;                       /* 1 : le relâché de Back appartient à Shift + Back */
    u32 previews;                       /* pré-écoutes lancées par nous (tests) */
    u32 mixes;                          /* blocs mixés avec gain (tests) */
};

static volatile struct br_state *br(void)
{
    volatile struct br_state *s = (volatile struct br_state *)(cfw_ram + 0x40);
    if (s->magic != BR_MAGIC) {
        s->magic = BR_MAGIC;
        s->loaded = 0;
        s->autoprev = 0;
        s->vol = VOL_DEFAULT;
        s->gain = VOL_DEFAULT * 32768u / 100u;
        s->dirty = 0;
        s->since = 0;
        s->held = 0;
        s->held_since = 0;
        s->last_repeat = 0;
        s->redraw = 0;
        s->eat_back = 0;
        s->previews = 0;
        s->mixes = 0;
    }
    return s;
}

static void set_vol(volatile struct br_state *s, u32 v)
{
    s->vol = v;
    s->gain = v * 32768u / 100u;
}

static void zero(u8 *p, u32 n)
{
    while (n--)
        *(volatile u8 *)p++ = 0;
}

static void load(volatile struct br_state *s)
{
    u8 f[FILE_OBJ] __attribute__((aligned(4)));
    u8 v[2] = {0, 0};
    s->loaded = 1;
    zero(f, FILE_OBJ);
    if (!fw_file_open(f, BR_PATH, FA_READ))
        return;
    if (fw_file_read(f, v, 2) == 2) {
        s->autoprev = v[0] ? 1u : 0u;
        set_vol(s, v[1] > 100 ? 100u : v[1]);
        s->redraw = 1;
    }
    fw_file_close(f);
}

static void save(volatile struct br_state *s)
{
    u8 f[FILE_OBJ] __attribute__((aligned(4)));
    u8 v[2];
    v[0] = (u8)s->autoprev;
    v[1] = (u8)s->vol;
    zero(f, FILE_OBJ);
    if (!fw_file_open(f, BR_PATH, FA_WRITE_NEW))
        return;
    fw_file_write(f, v, 2);
    fw_file_close(f);
}

static void changed(volatile struct br_state *s)
{
    s->dirty = 1;
    s->since = fw_millis;
}

/* L'écran navigateur s'il est affiché en mode liste de fichiers, sinon 0. */
static void *browser(void)
{
    void *ui = PTR(fw_input_listener, 8);
    void *scr;
    if (!ui)
        return 0;
    scr = PTR(ui, UI_BROWSER);
    if (!scr || PTR(ui, UI_CUR) != scr)
        return 0;
    if (U8(scr, BRW_MODE) != MODE_FILES || U8(scr, BRW_SUB) != 0)
        return 0;
    return scr;
}

/* « dossier/nom » dans out (256 o), comme le « %s/%s » de l'officiel. 0 si trop long. */
static int join(char *out, const char *dir, const char *name)
{
    u32 n = 0;
    while (*dir) {
        if (n >= 254)
            return 0;
        out[n++] = *dir++;
    }
    out[n++] = '/';
    while (*name) {
        if (n >= 255)
            return 0;
        out[n++] = *name++;
    }
    out[n] = 0;
    return 1;
}

static void preview_selected(volatile struct br_state *s, void *scr)
{
    char path[256];
    const char *const *names = (const char *const *)PTR(scr, BRW_NAMES);
    const char *dir = (const char *)PTR(scr, BRW_DIR);
    const char *name;
    if (!names || !dir)
        return;
    name = names[U32(scr, BRW_SEL)];
    if (!name || name[0] == '/' || name[0] == 0)
        return;                         /* dossier (ou « /.. ») : pas de pré-écoute */
    if (!join(path, dir, name))
        return;
    s->previews++;
    fw_preview_play(PTR(scr, BRW_PLAYER), path);
}

static void move(volatile struct br_state *s, void *l, void *scr, s32 d)
{
    fw_in_encoder(l, 0, (u32)d, 0);     /* l'officiel déplace la sélection, défile, redessine */
    if (s->autoprev)
        preview_selected(s, scr);
}

/* master_shift.c : boutons. 1 = avalé. */
int br_button(void *l, u32 a, u32 b, u32 c)
{
    volatile struct br_state *s = br();
    void *scr;
    (void)c;
    if (a == BTN_BACK && s->eat_back) {
        if (b == 1)
            s->eat_back = 0;
        return 1;
    }
    scr = browser();
    if (!scr)
        return 0;
    if (a == BTN_UP || a == BTN_DOWN) {
        if (b == 0) {
            move(s, l, scr, a == BTN_UP ? -1 : 1);
            s->held = a + 1;
            s->held_since = fw_millis;
            s->last_repeat = fw_millis;
        } else if (b == 1 && s->held == a + 1) {
            s->held = 0;
        }
        return 1;                       /* plus d'« entrée » sur B0/B3 ici : le clic d'encodeur reste */
    }
    if (a == BTN_BACK && b == 0 && fw_shift_held) {
        s->autoprev = !s->autoprev;
        s->eat_back = 1;
        changed(s);
        return 1;
    }
    return 0;
}

/* master_shift.c : potard 0 (libre). 1 = utilisé. */
int br_knob0(s32 d)
{
    volatile struct br_state *s = br();
    s32 v;
    if (!browser())
        return 0;
    v = (s32)s->vol + d;
    if (v < 0)
        v = 0;
    if (v > 100)
        v = 100;
    if ((u32)v != s->vol) {
        set_vol(s, (u32)v);
        s->redraw = 1;
        changed(s);
    }
    return 1;
}

/* settings_defer.c : à chaque tour de la boucle principale. */
void br_tick(void)
{
    volatile struct br_state *s = br();
    void *scr;
    if (!s->loaded)
        load(s);
    if (s->dirty && fw_millis - s->since >= SAVE_DELAY_MS) {
        s->dirty = 0;
        save(s);
    }
    if (s->held) {
        scr = browser();
        if (!scr) {
            s->held = 0;
        } else if (fw_millis - s->held_since >= REPEAT_DELAY_MS && fw_millis - s->last_repeat >= REPEAT_MS) {
            s->last_repeat = fw_millis;
            move(s, fw_input_listener, scr, s->held == BTN_UP + 1 ? -1 : 1);
        }
    }
}

/* Case 11 de l'écran navigateur : texte des touches. Shift tenu : Back montre l'auto-preview. */
const char *cfw_browser_label(void *screen, u32 key)
{
    if (key == 0 && fw_shift_held && browser() == screen)
        return br()->autoprev ? "Preview On" : "Preview Off";
    return fw_browser_label(screen, key);
}

/* Remplace fw_mix_add16 pour la pré-écoute (sous interruption audio : rapide, rien d'autre).
 * 64 trames : mot = gauche (16 bits bas) | droite (16 bits hauts), bus int32 gauche, droite. */
void cfw_preview_mix(s32 *bus, const u32 *src)
{
    volatile struct br_state *s = br();
    s32 g = (s32)s->gain;
    u32 i;
    s->mixes++;
    for (i = 0; i < 64; i++) {
        u32 w = src[i];
        bus[2 * i] += ((s32)(s16)(w & 0xffff) * g) >> 15;
        bus[2 * i + 1] += ((s32)(s16)(w >> 16) * g) >> 15;
    }
}

/* ---- jauge ---- */
#define DL_SAVE_CONTEXT 0x22000000u
#define DL_RESTORE_CONTEXT 0x23000000u
#define DL_RGB(c) (0x04000000u | ((c) & 0xffffff))
#define DL_BEGIN_RECTS 0x1f000009u
#define DL_END 0x21000000u
#define DL_VFORMAT_1PX 0x27000000u
#define DL_V(x, y) (0x40000000u | (((u32)(x) & 0x7fff) << 15) | ((u32)(y) & 0x7fff))
#define G_X0 4                          /* à gauche de la liste (la sélection commence à x = 22) */
#define G_X1 14
#define G_Y0 64
#define G_Y1 380
#define C_TRACK 0x2a2a2a
#define C_FILL 0xff4b32                 /* rouge proche de la sélection de l'officiel */

static void put(u32 w)
{
    fw_eve_word(w);
    fw_eve_wp = (u16)((fw_eve_wp + 4) & 0xfff);
    if (fw_eve_wp == 0) {               /* fin de l'anneau : rafale suivante au début (guest_screen.c) */
        fw_eve_end();
        fw_eve_begin();
    }
}

/* Remplace l'appel de fw_eve_display dans la composition : notre jauge par-dessus, puis DISPLAY. */
void cfw_overlay_display(void)
{
    if (browser()) {
        u32 v = br()->vol;
        s32 y = G_Y1 - (s32)((G_Y1 - G_Y0) * v / 100);
        put(DL_SAVE_CONTEXT);
        put(DL_VFORMAT_1PX);
        put(DL_BEGIN_RECTS);
        put(DL_RGB(C_TRACK));
        put(DL_V(G_X0, G_Y0));
        put(DL_V(G_X1, G_Y1));
        if (v) {
            put(DL_RGB(C_FILL));
            put(DL_V(G_X0, y));
            put(DL_V(G_X1, G_Y1));
        }
        put(DL_END);
        put(DL_RESTORE_CONTEXT);
    }
    fw_eve_display();
}

/* Remplace l'appel de l'automate d'affichage (bl en 0x600aab34) : recomposer quand la jauge change. */
void cfw_br_display_update(void *dm)
{
    volatile struct br_state *s = br();
    if (s->redraw && U32(dm, DM_READY) == 1 && U8(dm, DM_STATE) == 0) {
        s->redraw = 0;
        U8(dm, DM_STATE) = 2;
    }
    fw_display_update(dm);
}
