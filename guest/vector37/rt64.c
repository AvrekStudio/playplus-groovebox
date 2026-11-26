/* Divisions 64 bits de l'ABI ARM (__aeabi_uldivmod, __aeabi_ldivmod), absentes sans bibliothèque du
 * compilateur : Vector37 en fait quelques-unes (temps, interface ; pas dans le calcul du son au bloc).
 * Division par décalages et soustractions : lente mais rare. Les enveloppes en assembleur rendent le
 * quotient dans r0:r1 et le reste dans r2:r3, comme l'ABI le veut. */
#include <stdint.h>

uint64_t pp_udivmod64(uint64_t n, uint64_t d, uint64_t *rem)
{
    uint64_t q = 0, r = 0;
    int i;
    if (d == 0) {                               /* (comme une division par zéro sur la machine : pas de piège) */
        *rem = n;
        return 0;
    }
    if (n < d) {
        *rem = n;
        return 0;
    }
    if ((d >> 32) == 0 && (n >> 32) == 0) {     /* le cas courant : 32 bits, l'instruction du processeur */
        *rem = (uint32_t)n % (uint32_t)d;
        return (uint32_t)n / (uint32_t)d;
    }
    for (i = 63; i >= 0; i--) {
        r = r << 1 | (n >> i & 1u);
        if (r >= d) {
            r -= d;
            q |= (uint64_t)1 << i;
        }
    }
    *rem = r;
    return q;
}

int64_t pp_divmod64(int64_t n, int64_t d, int64_t *rem)
{
    uint64_t un = n < 0 ? (uint64_t)0 - (uint64_t)n : (uint64_t)n;
    uint64_t ud = d < 0 ? (uint64_t)0 - (uint64_t)d : (uint64_t)d, ur;
    uint64_t q = pp_udivmod64(un, ud, &ur);
    *rem = n < 0 ? -(int64_t)ur : (int64_t)ur; /* le reste a le signe du dividende (C99) */
    return (n < 0) != (d < 0) ? -(int64_t)q : (int64_t)q;
}

__asm__(".syntax unified\n"
        ".thumb\n"
        ".global __aeabi_uldivmod\n"
        ".type __aeabi_uldivmod, %function\n"
        ".thumb_func\n"
        "__aeabi_uldivmod:\n"
        "  push {r4, lr}\n"
        "  sub sp, sp, #16\n"
        "  add r4, sp, #8\n"
        "  str r4, [sp]\n"
        "  bl pp_udivmod64\n"
        "  ldrd r2, r3, [sp, #8]\n"
        "  add sp, sp, #16\n"
        "  pop {r4, pc}\n"
        ".global __aeabi_ldivmod\n"
        ".type __aeabi_ldivmod, %function\n"
        ".thumb_func\n"
        "__aeabi_ldivmod:\n"
        "  push {r4, lr}\n"
        "  sub sp, sp, #16\n"
        "  add r4, sp, #8\n"
        "  str r4, [sp]\n"
        "  bl pp_divmod64\n"
        "  ldrd r2, r3, [sp, #8]\n"
        "  add sp, sp, #16\n"
        "  pop {r4, pc}\n");
