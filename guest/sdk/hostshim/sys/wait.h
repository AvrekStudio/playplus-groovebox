#ifndef HOSTSHIM_WAIT_H
#define HOSTSHIM_WAIT_H
static inline int hs_wait(int *s) { (void)s; return -1; }
static inline int hs_waitpid(int p, int *s, int o) { (void)p; (void)s; (void)o; return -1; }
#define wait hs_wait
#define waitpid hs_waitpid
#define WIFEXITED(s) 1
#define WEXITSTATUS(s) 0
#endif
