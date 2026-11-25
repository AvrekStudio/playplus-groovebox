// Banc de mesure : rend N échantillons d'un modèle Braids, par tranches de 24 (taille max de Braids).
#include "braids/macro_oscillator.h"
using namespace braids;

static MacroOscillator osc;
static int16_t out[24];
static uint8_t sync[24];
volatile int32_t sink;

extern "C" void bench_init(void) {
  osc.Init();
}

extern "C" void bench_shape(int shape, int pitch, int p1, int p2) {
  osc.set_shape(static_cast<MacroOscillatorShape>(shape));
  osc.set_pitch(pitch);
  osc.set_parameters(p1, p2);
  osc.Strike();
}

extern "C" void bench_render(int n) {
  while (n > 0) {
    int k = n > 24 ? 24 : n;
    osc.Render(sync, out, k);
    sink += out[0];
    n -= k;
  }
}

extern "C" int bench_peak(void) {
  int p = 0;
  for (int i = 0; i < 24; ++i) { int v = out[i] < 0 ? -out[i] : out[i]; if (v > p) p = v; }
  return p;
}

// Minimal freestanding runtime
extern "C" void* memset(void* d, int c, size_t n) { unsigned char* p = (unsigned char*)d; while (n--) *p++ = (unsigned char)c; return d; }
extern "C" void* memcpy(void* d, const void* s, size_t n) { unsigned char* p = (unsigned char*)d; const unsigned char* q = (const unsigned char*)s; while (n--) *p++ = *q++; return d; }
extern "C" void* memmove(void* d, const void* s, size_t n) { unsigned char* p = (unsigned char*)d; const unsigned char* q = (const unsigned char*)s; if (p < q) while (n--) *p++ = *q++; else { p += n; q += n; while (n--) *--p = *--q; } return d; }
extern "C" void __cxa_pure_virtual() { while (1) {} }
extern "C" void abort() { while (1) {} }
extern "C" int __cxa_atexit(void (*)(void*), void*, void*) { return 0; }
void* __dso_handle = 0;
