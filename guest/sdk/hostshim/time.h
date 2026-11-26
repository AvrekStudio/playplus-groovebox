#ifndef HOSTSHIM_TIME_H
#define HOSTSHIM_TIME_H
typedef long time_t;
typedef long clock_t;
struct timespec { long tv_sec, tv_nsec; };
#define CLOCK_MONOTONIC 1
#define CLOCKS_PER_SEC 1000000
static inline int hs_clock_gettime(int c, struct timespec *t) { (void)c; t->tv_sec = 0; t->tv_nsec = 0; return 0; }
static inline clock_t hs_clock(void) { return 0; }
static inline time_t hs_time(time_t *t) { if (t) *t = 0; return 0; }
#define clock_gettime hs_clock_gettime
#define clock hs_clock
#define time hs_time
#endif
