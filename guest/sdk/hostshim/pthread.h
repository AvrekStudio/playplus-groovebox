#ifndef HOSTSHIM_PTHREAD_H
#define HOSTSHIM_PTHREAD_H
typedef int pthread_t, pthread_mutex_t, pthread_cond_t, pthread_attr_t;
#define PTHREAD_MUTEX_INITIALIZER 0
#define PTHREAD_COND_INITIALIZER 0
static inline int hs_pt0(void) { return 0; }
#define pthread_create(a, b, c, d) hs_pt0()
#define pthread_join(a, b) hs_pt0()
#define pthread_mutex_init(a, b) hs_pt0()
#define pthread_mutex_lock(a) hs_pt0()
#define pthread_mutex_unlock(a) hs_pt0()
#define pthread_cond_init(a, b) hs_pt0()
#define pthread_cond_wait(a, b) hs_pt0()
#define pthread_cond_signal(a) hs_pt0()
#define pthread_cond_broadcast(a) hs_pt0()
#define pthread_detach(a) hs_pt0()
#define pthread_self() 0
#define pthread_equal(a, b) ((a) == (b))
#define pthread_once(a, b) hs_pt0()
typedef int pthread_once_t;
#define PTHREAD_ONCE_INIT 0
#endif
