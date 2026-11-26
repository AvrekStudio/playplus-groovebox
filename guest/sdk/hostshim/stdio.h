/* En-têtes d'hôte « vides » pour compiler les harnais de test des firmwares Felucca dans un invité PGI : ce que
 * leurs tests appellent (fichiers, printf, fils, getenv) ne sert jamais dans l'invité ; ces stubs ne font rien. */
#ifndef HOSTSHIM_STDIO_H
#define HOSTSHIM_STDIO_H
#include <stddef.h>
#include <stdarg.h>
typedef struct hs_file { int x; } FILE;
#define stdin ((FILE *)0)
#define stdout ((FILE *)0)
#define stderr ((FILE *)0)
#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
static inline int hs_printf(const char *f, ...) { (void)f; return 0; }
static inline int hs_fprintf(FILE *s, const char *f, ...) { (void)s; (void)f; return 0; }
static inline int hs_snprintf(char *b, size_t n, const char *f, ...) { (void)f; if (n) b[0] = 0; return 0; }
static inline int hs_sprintf(char *b, const char *f, ...) { (void)f; b[0] = 0; return 0; }
static inline int hs_vsnprintf(char *b, size_t n, const char *f, va_list a) { (void)f; (void)a; if (n) b[0] = 0; return 0; }
static inline int hs_sscanf(const char *s, const char *f, ...) { (void)s; (void)f; return 0; }
static inline FILE *hs_fopen(const char *p, const char *m) { (void)p; (void)m; return 0; }
static inline int hs_fclose(FILE *f) { (void)f; return 0; }
static inline size_t hs_fwrite(const void *p, size_t s, size_t n, FILE *f) { (void)p; (void)s; (void)f; return n; }
static inline size_t hs_fread(void *p, size_t s, size_t n, FILE *f) { (void)p; (void)s; (void)n; (void)f; return 0; }
static inline int hs_fseek(FILE *f, long o, int w) { (void)f; (void)o; (void)w; return -1; }
static inline long hs_ftell(FILE *f) { (void)f; return 0; }
static inline int hs_fflush(FILE *f) { (void)f; return 0; }
static inline char *hs_fgets(char *b, int n, FILE *f) { (void)b; (void)n; (void)f; return 0; }
static inline int hs_fputc(int c, FILE *f) { (void)f; return c; }
static inline int hs_fputs(const char *s, FILE *f) { (void)s; (void)f; return 0; }
static inline int hs_puts(const char *s) { (void)s; return 0; }
static inline int hs_putchar(int c) { return c; }
static inline int hs_fgetc(FILE *f) { (void)f; return EOF; }
static inline void hs_perror(const char *s) { (void)s; }
static inline int hs_remove(const char *p) { (void)p; return -1; }
static inline int hs_setvbuf(FILE *f, char *b, int m, size_t n) { (void)f; (void)b; (void)m; (void)n; return 0; }
#define setvbuf hs_setvbuf
#define _IONBF 2
#define _IOLBF 1
#define _IOFBF 0
#define BUFSIZ 1024
#define printf hs_printf
#define fprintf hs_fprintf
#define snprintf hs_snprintf
#define sprintf hs_sprintf
#define vsnprintf hs_vsnprintf
#define sscanf hs_sscanf
#define fopen hs_fopen
#define fclose hs_fclose
#define fwrite hs_fwrite
#define fread hs_fread
#define fseek hs_fseek
#define ftell hs_ftell
#define fflush hs_fflush
#define fgets hs_fgets
#define fputc hs_fputc
#define putc hs_fputc
#define fputs hs_fputs
#define puts hs_puts
#define putchar hs_putchar
#define fgetc hs_fgetc
#define getc hs_fgetc
#define perror hs_perror
#define remove hs_remove
#endif
