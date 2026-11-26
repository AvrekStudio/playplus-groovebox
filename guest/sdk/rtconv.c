/* Conversions flottant <-> 64 bits que clang appelle sur le Cortex-M (pas de compiler-rt pour nos invités).
 * Code à nous (AvrekStudio). */
#include <stdint.h>
long long __aeabi_d2lz(double x)
{
    int neg = x < 0;
    unsigned long long hi, lo;
    if (neg)
        x = -x;
    if (x >= 9.2233720368547758e18)
        return neg ? (long long)0x8000000000000000ull : 0x7FFFFFFFFFFFFFFFll;
    hi = (unsigned long long)(uint32_t)(x / 4294967296.0);
    lo = (unsigned long long)(uint32_t)(x - (double)hi * 4294967296.0);
    return neg ? -(long long)(hi << 32 | lo) : (long long)(hi << 32 | lo);
}
unsigned long long __aeabi_d2ulz(double x)
{
    unsigned long long hi, lo;
    if (x <= 0)
        return 0;
    hi = (uint32_t)(x / 4294967296.0);
    lo = (uint32_t)(x - (double)hi * 4294967296.0);
    return hi << 32 | lo;
}
long long __aeabi_f2lz(float x) { return __aeabi_d2lz(x); }
unsigned long long __aeabi_f2ulz(float x) { return __aeabi_d2ulz(x); }
double __aeabi_ul2d(unsigned long long v) { return (double)(uint32_t)(v >> 32) * 4294967296.0 + (double)(uint32_t)v; }
double __aeabi_l2d(long long v) { return v < 0 ? -__aeabi_ul2d((unsigned long long)-v) : __aeabi_ul2d((unsigned long long)v); }
float __aeabi_ul2f(unsigned long long v) { return (float)__aeabi_ul2d(v); }
float __aeabi_l2f(long long v) { return (float)__aeabi_l2d(v); }
