/* La petite libc des invités compilés avec les harnais de test (guest/sdk/hostshim) : mem* et str*.
 * Code à nous (AvrekStudio). Les firmwares Felucca ont leur libc.c, renommée par leurs harnais
 * (memset -> felucca_memset) : celle-ci répond aux noms standard. */
#include <stddef.h>
void *memcpy(void *d, const void *s, size_t n) { unsigned char *a = d; const unsigned char *b = s; while (n--) *a++ = *b++; return d; }
void *memmove(void *d, const void *s, size_t n)
{
    unsigned char *a = d;
    const unsigned char *b = s;
    if (a < b) while (n--) *a++ = *b++;
    else { a += n, b += n; while (n--) *--a = *--b; }
    return d;
}
void *memset(void *d, int c, size_t n) { unsigned char *a = d; while (n--) *a++ = (unsigned char)c; return d; }
int memcmp(const void *x, const void *y, size_t n)
{
    const unsigned char *a = x, *b = y;
    for (; n; n--, a++, b++) if (*a != *b) return *a - *b;
    return 0;
}
size_t strlen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }
int strcmp(const char *a, const char *b) { while (*a && *a == *b) a++, b++; return (unsigned char)*a - (unsigned char)*b; }
int strncmp(const char *a, const char *b, size_t n)
{
    for (; n; n--, a++, b++) { if (*a != *b || !*a) return (unsigned char)*a - (unsigned char)*b; }
    return 0;
}
char *strcpy(char *d, const char *s) { char *r = d; while ((*d++ = *s++)) ; return r; }
char *strncpy(char *d, const char *s, size_t n) { char *r = d; while (n && *s) *d++ = *s++, n--; while (n--) *d++ = 0; return r; }
char *strcat(char *d, const char *s) { strcpy(d + strlen(d), s); return d; }
char *strchr(const char *s, int c) { for (; *s; s++) if (*s == (char)c) return (char *)s; return c ? 0 : (char *)s; }
char *strrchr(const char *s, int c) { const char *r = 0; for (; *s; s++) if (*s == (char)c) r = s; return c ? (char *)r : (char *)s; }
char *strstr(const char *h, const char *n)
{
    size_t k = strlen(n);
    for (; *h; h++) if (!strncmp(h, n, k)) return (char *)h;
    return k ? 0 : (char *)h;
}
