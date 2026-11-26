// Le strict nécessaire du C++ pour les invités PGI (Plaits, Elements… dans KARMA) : new / delete sur une petite
// réserve (ils servent à peine : les modèles prennent leur mémoire par placement), __cxa_pure_virtual.
// Code à nous (AvrekStudio).
#include <stddef.h>
static unsigned char pool[64 * 1024] __attribute__((aligned(8)));
static size_t used;
static void *grab(size_t n)
{
    void *p;
    n = (n + 7) & ~(size_t)7;
    if (used + n > sizeof pool)
        for (;;) ;
    p = pool + used;
    used += n;
    return p;
}
void *operator new(size_t n) { return grab(n); }
void *operator new[](size_t n) { return grab(n); }
void operator delete(void *) noexcept {}
void operator delete[](void *) noexcept {}
void operator delete(void *, size_t) noexcept {}
void operator delete[](void *, size_t) noexcept {}
extern "C" void __cxa_pure_virtual() { for (;;) ; }
extern "C" int __cxa_guard_acquire(long long *g) { return !*(char *)g; }
extern "C" void __cxa_guard_release(long long *g) { *(char *)g = 1; }
extern "C" void __cxa_guard_abort(long long *) {}
extern "C" int __aeabi_atexit(void *, void (*)(void *), void *) { return 0; }
void *__dso_handle = 0;
