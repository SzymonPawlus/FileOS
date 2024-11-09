//
// Created by szymon on 05/08/2021.
//

#include "strings.h"


int str_cmp(char* str1, char* str2){
    do
        if(!*str1  && !*str2) return 0;
    while(*str1++ == *str2++);
    return 1;
}

void str_cat(char* str1, char* str2){
    while(*str1) str1++;
    while(*str2) *str1++ = *str2++;
    *(++str1) = '\0';
}

int str_until(char* str, char x){
    int i = 0;
    while(*str != x) {
        if(!*str++) return 0;
        i++;
    }
    return i;
}

int str_cmp_until(char* str1, char* str2, char x){
    while(*str1 == *str2 || *str1 == x || *str1 == '\0'){
        if(*str1 == x || *str1 =='\0')
            return 0;
        str1++; str2++;
    }
    return 1;
}

void str_upper_case(char* str){
    while(*str){
        if(*str >= 'a' && *str <= 'z') *str -= 32;
        str++;
    }
}

void str_lower_case(char* str){
    while(*str){
        if(*str >= 'A' && *str <= 'Z') *str += 32;
        str++;
    }
}

void str_trail_char(char* str, char x){
    int i = 0, j = 0;
    while(str[i]){
        if(str[i] == x) {
            i++;
            continue;
        }
        str[j++] = str[i++];
    }

    while(j < i)
        str[j++] = 0;
}

void str_cpy(char* source, char* destination){
    while(*source)
        *(destination++) = *(source++);
}

void str_cpy_size(char* source, char* destination, uint32_t size){
    for (uint32_t i = 0; i < size && *source; ++i)
        *(destination++) = *(source++);
}

void str_insert(char* str, char x, uint16_t amount){
    int i = 0; char* begin = str;
    while(*str) { str++; i++; }
    for (; i >= 0 ; i--) {
        *(str + amount) = *(str);
        str--;
    }
    for (int j = 0; j < amount; ++j)
        *(begin++) = x;
}

uint32_t str_len(const char* str){
    uint32_t len = 0;
    while(*(str++)) len++;
    return len;
}

char* strchr(const char* str, char c) {
    while(*str){
        if(*str == c) return (char*)str;
        str++;
    }
    return 0;
}

char* strtok(char* str, char delim) {
    static char* last = 0;
    if(str) last = str;
    if(!last) return 0;
    char* begin = last;
    while(*last){
        if(*last == delim){
            *last = 0;
            last++;
            return begin;
        }
        last++;
    }
    return begin;
}
