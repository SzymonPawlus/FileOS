//
// Created by szymon on 02/08/2021.
//

#ifndef FILEOS_MEMORY_H
#define FILEOS_MEMORY_H

#include "../cpu/types.h"

void* k_malloc(uint32_t size);
void k_free(void* pointer);
void* k_realloc(void* p, uint32_t size);
void k_malloc_init();
void k_memset(void* memory, uint32_t size, uint8_t value);
int k_memcmp(void* mem1, void* mem2, uint32_t size);
void k_memreplace(uint8_t* mem, char to_replace, char to_replace_with, uint32_t size);

#endif //FILEOS_MEMORY_H
