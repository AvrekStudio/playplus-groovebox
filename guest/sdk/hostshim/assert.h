#ifndef HOSTSHIM_ASSERT_H
#define HOSTSHIM_ASSERT_H
#define assert(x) ((void)0)
#define static_assert _Static_assert
#endif
