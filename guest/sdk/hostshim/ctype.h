#ifndef HOSTSHIM_CTYPE_H
#define HOSTSHIM_CTYPE_H
static inline int hs_isdigit(int c) { return c >= '0' && c <= '9'; }
static inline int hs_isspace(int c) { return c == ' ' || (c >= 9 && c <= 13); }
static inline int hs_toupper(int c) { return c >= 'a' && c <= 'z' ? c - 32 : c; }
static inline int hs_tolower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
static inline int hs_isalpha(int c) { return (c | 32) >= 'a' && (c | 32) <= 'z'; }
static inline int hs_isprint(int c) { return c >= 32 && c < 127; }
#define isdigit hs_isdigit
#define isspace hs_isspace
#define toupper hs_toupper
#define tolower hs_tolower
#define isalpha hs_isalpha
#define isprint hs_isprint
#endif
