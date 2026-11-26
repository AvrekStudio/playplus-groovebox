/* Lecture, par un invité, de sa matrice de câblage sur la carte (JSON « fm1-playplus-map/1 » de la page de
 * câblage, copié en /Guests/<nom de l'invité>/map.json) : changer de matrice sans reconstruire le firmware.
 *
 * Lecteur JSON minimal, sans allocation ni bibliothèque : il parcourt le texte, ne garde des liens que
 * « fm1 », « playplus », « shift » et « action » (« cran+ », « cran- » ou {"value": N}), et des couleurs
 * de pads (« pad_colors ») « pad_index » et « color » (#rrggbb). Tout le reste est sauté. Chaque lien est
 * donné à un rappel de l'invité, qui le traduit dans ses tables (ses noms de contrôles à lui).
 * À inclure dans le fichier de la couche de l'invité (fonctions static). Boucle principale seulement. */
#ifndef MAPJSON_H
#define MAPJSON_H
#include <stdint.h>

enum { MJ_PAD, MJ_BTN, MJ_KNOB, MJ_ENC };            /* la source côté Play+ */
enum { MJ_ACT_NONE, MJ_ACT_INC, MJ_ACT_DEC, MJ_ACT_VAL };

struct mj_link {
    char fm1[28];                                    /* le contrôle du firmware, tel que la page l'écrit */
    uint8_t kind, idx, shift, act;                   /* MJ_PAD 0..159, MJ_BTN 0..11, MJ_KNOB 0..14, MJ_ENC 0 */
    int32_t val;                                     /* « valeur N » */
};

struct mj_state {
    const char *p, *end;
    int bad;
};

static void mj_ws(struct mj_state *s)
{
    while (s->p < s->end && (*s->p == ' ' || *s->p == '\n' || *s->p == '\r' || *s->p == '\t'))
        s->p++;
}

/* une chaîne : copiée (tronquée à max - 1, sans les échappements \u) dans out si out, sautée sinon */
static int mj_str(struct mj_state *s, char *out, uint32_t max)
{
    uint32_t n = 0;
    mj_ws(s);
    if (s->p >= s->end || *s->p != '"')
        return s->bad = 1, 0;
    s->p++;
    while (s->p < s->end && *s->p != '"') {
        char c = *s->p++;
        if (c == '\\' && s->p < s->end) {
            c = *s->p++;
            if (c == 'u') {                          /* \uXXXX : remplacé par « ? » */
                s->p += 4;
                c = '?';
            } else if (c == 'n')
                c = '\n';
        }
        if (out && n + 1 < max)
            out[n++] = c;
    }
    if (s->p >= s->end)
        return s->bad = 1, 0;
    s->p++;
    if (out)
        out[n] = 0;
    return 1;
}

static int32_t mj_num(struct mj_state *s)
{
    int32_t v = 0, neg = 0;
    mj_ws(s);
    if (s->p < s->end && *s->p == '-')
        neg = 1, s->p++;
    while (s->p < s->end && *s->p >= '0' && *s->p <= '9')
        v = v * 10 + (*s->p++ - '0');
    while (s->p < s->end && ((*s->p >= '0' && *s->p <= '9') || *s->p == '.' || *s->p == 'e' || *s->p == 'E' ||
                             *s->p == '+' || *s->p == '-'))
        s->p++;                                      /* (décimales : ignorées) */
    return neg ? -v : v;
}

static void mj_skip(struct mj_state *s, int depth)  /* une valeur quelconque */
{
    mj_ws(s);
    if (s->p >= s->end || depth > 16) {
        s->bad = 1;
        return;
    }
    if (*s->p == '"')
        mj_str(s, 0, 0);
    else if (*s->p == '{' || *s->p == '[') {
        char close = *s->p == '{' ? '}' : ']';
        s->p++;
        mj_ws(s);
        if (s->p < s->end && *s->p == close) {
            s->p++;
            return;
        }
        while (!s->bad && s->p < s->end) {
            if (close == '}') {
                mj_str(s, 0, 0);
                mj_ws(s);
                if (s->p >= s->end || *s->p != ':') {
                    s->bad = 1;
                    return;
                }
                s->p++;
            }
            mj_skip(s, depth + 1);
            mj_ws(s);
            if (s->p < s->end && *s->p == ',') {
                s->p++;
                continue;
            }
            if (s->p < s->end && *s->p == close) {
                s->p++;
                return;
            }
            s->bad = 1;
        }
    } else if (*s->p == 't')
        s->p += 4;
    else if (*s->p == 'f')
        s->p += 5;
    else if (*s->p == 'n')
        s->p += 4;
    else
        mj_num(s);
}

static int mj_eq(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}

/* "pad:12", "btn:7", "knob:3", "enc" -> le lien ; 0 si inconnu */
static int mj_source(const char *t, struct mj_link *l)
{
    const char *n = t;
    int32_t v = 0;
    while (*n && *n != ':')
        n++;
    if (*n == ':')
        for (n++; *n >= '0' && *n <= '9'; n++)
            v = v * 10 + (*n - '0');
    if (t[0] == 'p' && t[1] == 'a' && t[2] == 'd' && t[3] == ':' && v < 160)
        l->kind = MJ_PAD;
    else if (t[0] == 'b' && t[1] == 't' && t[2] == 'n' && t[3] == ':' && v < 12)
        l->kind = MJ_BTN;
    else if (t[0] == 'k' && t[1] == 'n' && t[2] == 'o' && t[3] == 'b' && t[4] == ':' && v < 15)
        l->kind = MJ_KNOB;
    else if (mj_eq(t, "enc"))
        l->kind = MJ_ENC, v = 0;
    else
        return 0;
    l->idx = (uint8_t)v;
    return 1;
}

static uint32_t mj_hex(const char *h)               /* "#rrggbb" -> 0xrrggbb, 0xff000000 si illisible */
{
    uint32_t v = 0, i;
    if (h[0] != '#')
        return 0xff000000u;
    for (i = 1; i <= 6; i++) {
        char c = h[i];
        v = v << 4 | (uint32_t)(c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10
                                : c >= 'A' && c <= 'F' ? c - 'A' + 10 : 99);
        if (v & 0xf0000000u)
            return 0xff000000u;
    }
    return v;
}

typedef int (*mj_link_fn)(const struct mj_link *l);  /* 1 : pris ; 0 : refusé (contrôle inconnu…) */
typedef void (*mj_color_fn)(uint32_t pad, uint32_t rgb);

/* Parcourt la matrice. Rend le nombre de liens pris, -1 si le texte n'est pas une matrice lisible ; *refused :
 * les liens que l'invité n'a pas compris (pour l'afficher). */
static int mj_parse(const char *txt, uint32_t n, mj_link_fn on_link, mj_color_fn on_color, int *refused)
{
    struct mj_state s = {txt, txt + n, 0};
    char key[24];
    int taken = 0, fmt = 0;
    *refused = 0;
    mj_ws(&s);
    if (s.p >= s.end || *s.p != '{')
        return -1;
    s.p++;
    while (!s.bad && s.p < s.end) {
        mj_ws(&s);
        if (*s.p == '}')
            break;
        if (!mj_str(&s, key, sizeof key))
            break;
        mj_ws(&s);
        if (s.p >= s.end || *s.p != ':')
            return -1;
        s.p++;
        mj_ws(&s);
        if (mj_eq(key, "format")) {
            char f[32];
            mj_str(&s, f, sizeof f);
            fmt = mj_eq(f, "fm1-playplus-map/1");
        } else if ((mj_eq(key, "links") || mj_eq(key, "pad_colors")) && s.p < s.end && *s.p == '[') {
            int links = mj_eq(key, "links");
            s.p++;
            mj_ws(&s);
            while (!s.bad && s.p < s.end && *s.p != ']') {
                struct mj_link l;
                char src[16] = "", col[12] = "";
                int32_t padi = -1;
                int ok_src = 0, i;
                for (i = 0; i < (int)sizeof l; i++)
                    ((char *)&l)[i] = 0;
                mj_ws(&s);
                if (s.p >= s.end || *s.p != '{') {
                    s.bad = 1;
                    break;
                }
                s.p++;
                while (!s.bad && s.p < s.end) {
                    char k[16];
                    mj_ws(&s);
                    if (*s.p == '}') {
                        s.p++;
                        break;
                    }
                    mj_str(&s, k, sizeof k);
                    mj_ws(&s);
                    if (s.p >= s.end || *s.p != ':') {
                        s.bad = 1;
                        break;
                    }
                    s.p++;
                    mj_ws(&s);
                    if (mj_eq(k, "fm1"))
                        mj_str(&s, l.fm1, sizeof l.fm1);
                    else if (mj_eq(k, "playplus"))
                        mj_str(&s, src, sizeof src);
                    else if (mj_eq(k, "shift")) {
                        l.shift = s.p < s.end && *s.p == 't';
                        mj_skip(&s, 0);
                    } else if (mj_eq(k, "action") && *s.p == '"') {
                        char a[8];
                        mj_str(&s, a, sizeof a);
                        l.act = mj_eq(a, "cran+") ? MJ_ACT_INC : mj_eq(a, "cran-") ? MJ_ACT_DEC : MJ_ACT_NONE;
                    } else if (mj_eq(k, "action") && *s.p == '{') {   /* {"value": N} */
                        char vk[8];
                        s.p++;
                        mj_str(&s, vk, sizeof vk);
                        mj_ws(&s);
                        if (s.p < s.end && *s.p == ':')
                            s.p++;
                        l.val = mj_num(&s);
                        l.act = mj_eq(vk, "value") ? MJ_ACT_VAL : MJ_ACT_NONE;
                        mj_ws(&s);
                        if (s.p < s.end && *s.p == '}')
                            s.p++;
                    } else if (mj_eq(k, "pad_index"))
                        padi = mj_num(&s);
                    else if (mj_eq(k, "color"))
                        mj_str(&s, col, sizeof col);
                    else
                        mj_skip(&s, 0);
                    mj_ws(&s);
                    if (s.p < s.end && *s.p == ',')
                        s.p++;
                }
                if (links) {
                    ok_src = mj_source(src, &l);
                    if (ok_src && l.fm1[0] && on_link(&l))
                        taken++;
                    else
                        (*refused)++;
                } else if (padi >= 0 && padi < 160 && on_color && mj_hex(col) != 0xff000000u)
                    on_color((uint32_t)padi, mj_hex(col));
                mj_ws(&s);
                if (s.p < s.end && *s.p == ',')
                    s.p++;
                mj_ws(&s);
            }
            if (s.p < s.end && *s.p == ']')
                s.p++;
        } else
            mj_skip(&s, 0);
        mj_ws(&s);
        if (s.p < s.end && *s.p == ',')
            s.p++;
    }
    if (s.bad || !fmt)
        return -1;
    return taken;
}
#endif
