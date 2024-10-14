//
// Created by szymon on 02/08/2021.
//

#ifndef FILEOS_MEMORY_H
#define FILEOS_MEMORY_H

#include "../cpu/types.h"

void* k_malloc(u32 size);
void k_free(void* pointer);
void* k_realloc(void* p, u32 size);
void k_malloc_init();
void k_memset(void* memory, u32 size, u8 value);
int k_memcmp(void* mem1, void* mem2, u32 size);
void k_memreplace(u8* mem, char to_replace, char to_replace_with, u32 size);

#endif //FILEOS_MEMORY_H
