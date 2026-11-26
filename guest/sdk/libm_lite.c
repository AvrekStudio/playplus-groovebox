/* Une petite libm pour les invités PGI (le Play+ n'a pas de libm pour nos invités) : sin, cos, tan, exp, log, pow,
 * atan, atan2, tanh, fmod… en double (le M7 du Play+ a l'unité flottante double), et leurs versions float.
 * Code à nous (AvrekStudio). Précision visée ~1e-12 en double : assez pour les tables que les moteurs calculent au
 * démarrage (Plaits, Elements…) et pour les rares appels au fil du son. Réduction d'argument simple (pas pour des
 * arguments énormes). */
#include <stdint.h>

#define PI 3.14159265358979323846
#define PI_2 1.57079632679489661923
#define LN2 0.69314718055994530942
#define LOG2E 1.44269504088896340736

static double poly_sin(double x)                /* |x| <= pi/4 */
{
    double x2 = x * x;
    return x * (1.0 + x2 * (-1.0 / 6 + x2 * (1.0 / 120 + x2 * (-1.0 / 5040 + x2 * (1.0 / 362880 + x2 * (-1.0 / 39916800
           + x2 * (1.0 / 6227020800.0)))))));
}
static double poly_cos(double x)
{
    double x2 = x * x;
    return 1.0 + x2 * (-0.5 + x2 * (1.0 / 24 + x2 * (-1.0 / 720 + x2 * (1.0 / 40320 + x2 * (-1.0 / 3628800
           + x2 * (1.0 / 479001600.0 + x2 * (-1.0 / 87178291200.0)))))));
}
static double rnd(double x) { return __builtin_round(x); }
double ldexp(double x, int e);

double sin(double x)
{
    double k = rnd(x / PI_2), r = x - k * PI_2;
    switch (((long long)k) & 3) {
    case 0: return poly_sin(r);
    case 1: return poly_cos(r);
    case 2: return -poly_sin(r);
    default: return -poly_cos(r);
    }
}
double cos(double x) { return sin(x + PI_2); }
double tan(double x)
{
    double k = rnd(x / PI_2), r = x - k * PI_2, s = poly_sin(r), c = poly_cos(r);
    return (((long long)k) & 1) ? -c / s : s / c;
}
double exp(double x)
{
    double k, r, s, t;
    int i;
    union { double d; uint64_t u; } v;
    if (x > 709.0)
        return __builtin_inf();
    if (x < -745.0)
        return 0.0;
    k = rnd(x * LOG2E);
    r = x - k * LN2;                            /* |r| <= ln2/2 */
    s = 1.0, t = 1.0;
    for (i = 1; i < 16; i++) {
        t *= r / i;
        s += t;
    }
    if (k < -1020 || k > 1020)
        return ldexp(s, (int)k);
    v.u = (uint64_t)((long long)k + 1023) << 52;
    return s * v.d;
}
double log(double x)
{
    union { double d; uint64_t u; } v;
    int e;
    double m, z, z2, s, t;
    int i;
    if (x <= 0.0)
        return x == 0.0 ? -__builtin_inf() : __builtin_nan("");
    if (__builtin_isinf(x))
        return x;
    v.d = x;
    e = (int)((v.u >> 52) & 0x7FF);
    if (!e) {                                   /* sous-normal */
        v.d = x * 18014398509481984.0;          /* 2^54 */
        e = (int)((v.u >> 52) & 0x7FF) - 54;
    }
    e -= 1023;
    v.u = (v.u & 0x000FFFFFFFFFFFFFull) | 0x3FF0000000000000ull;
    m = v.d;                                    /* [1, 2) */
    if (m > 1.41421356237309504880) {
        m *= 0.5;
        e++;
    }
    z = (m - 1.0) / (m + 1.0);                  /* ln m = 2 atanh z, |z| <= 0.172 */
    z2 = z * z;
    s = 0.0, t = z;
    for (i = 1; i < 30; i += 2) {
        s += t / i;
        t *= z2;
    }
    return 2.0 * s + e * LN2;
}
double log2(double x) { return log(x) * LOG2E; }
double log10(double x) { return log(x) * 0.43429448190325182765; }
double exp2(double x) { return exp(x * LN2); }
double pow(double x, double y)
{
    if (y == 0.0)
        return 1.0;
    if (x == 0.0)
        return y > 0 ? 0.0 : __builtin_inf();
    if (x < 0.0) {
        double yi = rnd(y);
        if (yi != y)
            return __builtin_nan("");
        return (((long long)yi) & 1) ? -exp(y * log(-x)) : exp(y * log(-x));
    }
    return exp(y * log(x));
}
double atan(double x)
{
    int neg = x < 0, inv = 0, red = 0;
    double s, t, x2;
    int i;
    if (neg)
        x = -x;
    if (x > 1.0)
        x = 1.0 / x, inv = 1;
    if (x > 0.26794919243112270) {               /* tan(pi/12) : atan x = pi/6 + atan((x√3-1)/(x+√3)) */
        x = (x * 1.73205080756887729 - 1.0) / (x + 1.73205080756887729);
        red = 1;
    }
    x2 = x * x, s = 0.0, t = x;
    for (i = 1; i < 40; i += 2) {
        s += ((i >> 1) & 1 ? -t : t) / i;
        t *= x2;
    }
    if (red)
        s += PI / 6;
    if (inv)
        s = PI_2 - s;
    return neg ? -s : s;
}
double atan2(double y, double x)
{
    if (x > 0)
        return atan(y / x);
    if (x < 0)
        return y >= 0 ? atan(y / x) + PI : atan(y / x) - PI;
    return y > 0 ? PI_2 : y < 0 ? -PI_2 : 0.0;
}
double asin(double x) { return atan2(x, __builtin_sqrt(1.0 - x * x)); }
double acos(double x) { return atan2(__builtin_sqrt(1.0 - x * x), x); }
double tanh(double x)
{
    double e;
    if (x > 20.0)
        return 1.0;
    if (x < -20.0)
        return -1.0;
    e = exp(2.0 * x);
    return (e - 1.0) / (e + 1.0);
}
double sinh(double x) { double e = exp(x); return 0.5 * (e - 1.0 / e); }
double cosh(double x) { double e = exp(x); return 0.5 * (e + 1.0 / e); }
double fmod(double x, double y)
{
    double q;
    if (y == 0.0)
        return __builtin_nan("");
    q = x / y;
    q = q < 0 ? __builtin_ceil(q) : __builtin_floor(q);
    return x - q * y;
}
double modf(double x, double *ip) { double i = x < 0 ? __builtin_ceil(x) : __builtin_floor(x); *ip = i; return x - i; }
double frexp(double x, int *e)
{
    union { double d; uint64_t u; } v;
    int ee;
    if (x == 0.0 || __builtin_isinf(x) || __builtin_isnan(x)) {
        *e = 0;
        return x;
    }
    v.d = x;
    ee = (int)((v.u >> 52) & 0x7FF);
    if (!ee) {
        v.d = x * 18014398509481984.0;
        ee = (int)((v.u >> 52) & 0x7FF) - 54;
    }
    *e = ee - 1022;
    v.u = (v.u & 0x800FFFFFFFFFFFFFull) | 0x3FE0000000000000ull;
    return v.d;
}
double ldexp(double x, int e)
{
    while (e > 1000)
        x *= 1.0715086071862673e301, e -= 1000;  /* 2^1000 */
    while (e < -1000)
        x *= 9.332636185032189e-302, e += 1000;  /* 2^-1000 */
    {
        union { double d; uint64_t u; } v;
        v.u = (uint64_t)(e + 1023) << 52;
        return x * v.d;
    }
}
double cbrt(double x) { return x < 0 ? -pow(-x, 1.0 / 3) : pow(x, 1.0 / 3); }
double hypot(double x, double y) { return __builtin_sqrt(x * x + y * y); }

float sinf(float x) { return (float)sin(x); }
float cosf(float x) { return (float)cos(x); }
float tanf(float x) { return (float)tan(x); }
float expf(float x) { return (float)exp(x); }
float exp2f(float x) { return (float)exp2(x); }
float logf(float x) { return (float)log(x); }
float log2f(float x) { return (float)log2(x); }
float log10f(float x) { return (float)log10(x); }
float powf(float x, float y) { return (float)pow(x, y); }
float atanf(float x) { return (float)atan(x); }
float atan2f(float y, float x) { return (float)atan2(y, x); }
float asinf(float x) { return (float)asin(x); }
float acosf(float x) { return (float)acos(x); }
float tanhf(float x) { return (float)tanh(x); }
float sinhf(float x) { return (float)sinh(x); }
float coshf(float x) { return (float)cosh(x); }
float fmodf(float x, float y) { return (float)fmod(x, y); }
float modff(float x, float *ip) { double i; float r = (float)modf(x, &i); *ip = (float)i; return r; }
float frexpf(float x, int *e) { return (float)frexp(x, e); }
float ldexpf(float x, int e) { return (float)ldexp(x, e); }
float cbrtf(float x) { return (float)cbrt(x); }
float hypotf(float x, float y) { return __builtin_sqrtf(x * x + y * y); }
float expm1f(float x) { return (float)(exp(x) - 1.0); }
float log1pf(float x) { return (float)log(1.0 + x); }
double expm1(double x) { return exp(x) - 1.0; }
double log1p(double x) { return log(1.0 + x); }

/* (les fonctions d'une instruction, aussi en vrai symbole : la partie C++ les appelle sans les en-têtes inline) */
float sqrtf(float x) { return __builtin_sqrtf(x); }
float fabsf(float x) { return __builtin_fabsf(x); }
float floorf(float x) { return __builtin_floorf(x); }
float ceilf(float x) { return __builtin_ceilf(x); }
float roundf(float x) { return __builtin_roundf(x); }
float truncf(float x) { return __builtin_truncf(x); }
float fminf(float a, float b) { return __builtin_fminf(a, b); }
float fmaxf(float a, float b) { return __builtin_fmaxf(a, b); }
double sqrt(double x) { return __builtin_sqrt(x); }
double fabs(double x) { return __builtin_fabs(x); }
double floor(double x) { return __builtin_floor(x); }
double ceil(double x) { return __builtin_ceil(x); }
double round(double x) { return __builtin_round(x); }
double trunc(double x) { return __builtin_trunc(x); }
