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
void str_cpy_size(char* source, char* destination, uint32_t size);
void str_insert(char* str, char x, uint16_t amount);
uint32_t str_len(const char* str);
char* strchr(const char* str, char c);
char* strtok(char* str, char delim);


#endif //FILEOS_STRINGS_H
