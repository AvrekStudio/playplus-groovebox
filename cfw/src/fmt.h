/* Petites fonctions de mise en forme de texte (pas de libc sur la machine, donc pas de sprintf).
 * Toutes écrivent à partir de p et renvoient la position suivante ; l'appelant termine par *p = 0. */
#ifndef FMT_H
#define FMT_H

#include "cfw.h"

static inline char *fmt_str(char *p, const char *s)
{
    while (*s)
        *p++ = *s++;
    return p;
}

static inline char *fmt_uint(char *p, u32 v)
{
    char t[10];
    u32 n = 0;
    do {
        t[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v);
    while (n)
        *p++ = t[--n];
    return p;
}

/* v en dixièmes : 247 -> "24.7" */
static inline char *fmt_tenths(char *p, u32 v)
{
    p = fmt_uint(p, v / 10u);
    *p++ = '.';
    *p++ = (char)('0' + v % 10u);
    return p;
}

#endif
