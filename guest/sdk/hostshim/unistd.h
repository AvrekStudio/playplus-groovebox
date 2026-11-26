#ifndef HOSTSHIM_UNISTD_H
#define HOSTSHIM_UNISTD_H
#include <sys/types.h>
static inline long hs_read(int f, void *b, unsigned long n) { (void)f; (void)b; (void)n; return -1; }
static inline long hs_write(int f, const void *b, unsigned long n) { (void)f; (void)b; return (long)n; }
static inline int hs_close(int f) { (void)f; return 0; }
static inline int hs_pipe(int p[2]) { p[0] = p[1] = -1; return -1; }
static inline void hs__exit(int c) { (void)c; for (;;) ; }
static inline int hs_getpid(void) { return 1; }
#define read hs_read
#define write hs_write
#define close hs_close
#define pipe hs_pipe
#define _exit hs__exit
#define getpid hs_getpid
static inline int hs_usleep(unsigned u) { (void)u; return 0; }
static inline unsigned hs_sleep(unsigned u) { (void)u; return 0; }
static inline int hs_fork(void) { return -1; }
static inline int hs_isatty(int f) { (void)f; return 0; }
#define usleep hs_usleep
#define sleep hs_sleep
#define fork hs_fork
#define isatty hs_isatty
#endif
