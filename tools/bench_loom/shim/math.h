#pragma once
#define M_PI 3.14159265358979323846
static inline float fabsf(float x){return __builtin_fabsf(x);}
static inline float sqrtf(float x){return __builtin_sqrtf(x);}
static inline float floorf(float x){return __builtin_floorf(x);}
static inline float ceilf(float x){return __builtin_ceilf(x);}
static inline float roundf(float x){return __builtin_roundf(x);}
static inline float truncf(float x){return __builtin_truncf(x);}
static inline float fminf(float a,float b){return __builtin_fminf(a,b);}
static inline float fmaxf(float a,float b){return __builtin_fmaxf(a,b);}
static inline long lrintf(float x){return (long)__builtin_rintf(x);}
#define isnan(x) __builtin_isnan(x)
#define isinf(x) __builtin_isinf(x)
#define isfinite(x) __builtin_isfinite(x)
