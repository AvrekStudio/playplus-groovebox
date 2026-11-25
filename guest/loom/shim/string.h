#pragma once
#include <stddef.h>
void *memset(void *, int, size_t); void *memcpy(void *, const void *, size_t); void *memmove(void *, const void *, size_t);
size_t strlen(const char *); int strcmp(const char *, const char *); int memcmp(const void *, const void *, size_t);
char *strncpy(char *, const char *, size_t); int strncmp(const char *, const char *, size_t);
