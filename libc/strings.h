//
// Created by szymon on 05/08/2021.
//

#ifndef FILEOS_STRINGS_H
#define FILEOS_STRINGS_H

#include "../cpu/types.h"

int str_cmp(char* str1, char* str2);
void str_cat(char* str1, char* str2);
int str_until(char* str, char x);
int str_cmp_until(char* str1, char* str2, char x);
void str_upper_case(char* str);
void str_lower_case(char* str);
void str_trail_char(char* str, char x);
void str_cpy(char* source, char* destination);
void str_cpy_size(char* source, char* destination, u32 size);
void str_insert(char* str, char x, u16 amount);
u32 str_len(char* str);


#endif //FILEOS_STRINGS_H
