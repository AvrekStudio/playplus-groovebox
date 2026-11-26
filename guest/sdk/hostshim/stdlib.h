#ifndef HOSTSHIM_STDLIB_H
#define HOSTSHIM_STDLIB_H
#include <stddef.h>
static inline void *hs_malloc(size_t n) { (void)n; return 0; }
static inline void *hs_calloc(size_t a, size_t b) { (void)a; (void)b; return 0; }
static inline void *hs_realloc(void *p, size_t n) { (void)p; (void)n; return 0; }
static inline void hs_free(void *p) { (void)p; }
static inline char *hs_getenv(const char *n) { (void)n; return 0; }
static inline int hs_atoi(const char *s) { int v = 0, g = 1; if (!s) return 0; if (*s == '-') g = -1, s++; while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0'); return g * v; }
static inline long hs_strtol(const char *s, char **e, int b) { (void)b; if (e) *e = (char *)s; return hs_atoi(s); }
static inline double hs_atof(const char *s) { return hs_atoi(s); }
static inline void hs_abort(void) { for (;;) ; }
static inline void hs_exit(int c) { (void)c; for (;;) ; }
static inline int hs_rand(void) { static unsigned s = 1; s = s * 1103515245u + 12345u; return (int)((s >> 16) & 0x7FFF); }
static inline void hs_srand(unsigned s) { (void)s; }
static inline int hs_abs(int v) { return v < 0 ? -v : v; }
static inline void hs_qsort(void *b, size_t n, size_t s, int (*c)(const void *, const void *)) { (void)b; (void)n; (void)s; (void)c; }
#define malloc hs_malloc
#define calloc hs_calloc
#define realloc hs_realloc
#define free hs_free
#define getenv hs_getenv
#define atoi hs_atoi
#define atol hs_atoi
#define strtol hs_strtol
#define strtoul hs_strtol
#define atof hs_atof
#define abort hs_abort
#define exit hs_exit
#define rand hs_rand
#define srand hs_srand
#define qsort hs_qsort
#define abs hs_abs
#define labs hs_abs
#define RAND_MAX 0x7FFF
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#endif
