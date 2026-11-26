#ifndef HOSTSHIM_MATH_H
#define HOSTSHIM_MATH_H
#ifdef __cplusplus
extern "C" {
#endif
/* (comme guest/loom/shim/math.h : les fonctions qui se font en une instruction ; le reste de libm n'est appelé
 * que par les tests des firmwares, jamais dans l'invité : des bouchons) */
#define M_PI 3.14159265358979323846
#define M_SQRT2 1.41421356237309504880
#define M_PI_2 1.57079632679489661923
#define M_LN2 0.69314718055994530942
#define M_E 2.7182818284590452354
#define HUGE_VALF __builtin_huge_valf()
#define INFINITY __builtin_inff()
#define NAN __builtin_nanf("")
static inline float fabsf(float x) { return __builtin_fabsf(x); }
static inline float sqrtf(float x) { return __builtin_sqrtf(x); }
static inline float floorf(float x) { return __builtin_floorf(x); }
static inline float ceilf(float x) { return __builtin_ceilf(x); }
static inline float roundf(float x) { return __builtin_roundf(x); }
static inline float truncf(float x) { return __builtin_truncf(x); }
static inline float fminf(float a, float b) { return __builtin_fminf(a, b); }
static inline float fmaxf(float a, float b) { return __builtin_fmaxf(a, b); }
static inline long lrintf(float x) { return (long)__builtin_rintf(x); }
static inline float rintf(float x) { return __builtin_rintf(x); }
static inline double fabs(double x) { return __builtin_fabs(x); }
static inline double sqrt(double x) { return __builtin_sqrt(x); }
static inline double floor(double x) { return __builtin_floor(x); }
static inline double ceil(double x) { return __builtin_ceil(x); }
double sin(double), cos(double), tan(double), exp(double), exp2(double), log(double), log2(double), log10(double);
double pow(double, double), atan(double), atan2(double, double), asin(double), acos(double), tanh(double);
double sinh(double), cosh(double), fmod(double, double), modf(double, double *), frexp(double, int *);
double ldexp(double, int), cbrt(double), hypot(double, double), expm1(double), log1p(double);
float sinf(float), cosf(float), tanf(float), expf(float), exp2f(float), logf(float), log2f(float), log10f(float);
float powf(float, float), atanf(float), atan2f(float, float), asinf(float), acosf(float), tanhf(float);
float sinhf(float), coshf(float), fmodf(float, float), modff(float, float *), frexpf(float, int *);
float ldexpf(float, int), cbrtf(float), hypotf(float, float), expm1f(float), log1pf(float);
#define round(x) __builtin_round(x)
#define isnan(x) __builtin_isnan(x)
#define isinf(x) __builtin_isinf(x)
#define isfinite(x) __builtin_isfinite(x)
#ifdef __cplusplus
}
#endif
#endif
