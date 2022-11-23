#ifndef _DEPENDENCIES_H
#define _DEPENDENCIES_H
#include "config.h"

void * __malloc(u64 size);
void __free(void * ptr);
int __strcmp(const char * str1, const char * str2);
#endif