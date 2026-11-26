#ifndef HOSTSHIM_STAT_H
#define HOSTSHIM_STAT_H
static inline int hs_mkdir(const char *p, int m) { (void)p; (void)m; return 0; }
#define mkdir hs_mkdir
#endif
