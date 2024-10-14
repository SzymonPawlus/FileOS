//
// Created by szymon on 31/07/2021.
//

#include "util.h"
#include "../cpu/types.h"

void k_memcpy(void *source, void *dest, int bytes){
    u8* s = source;u8* d = dest;
    for (int i = 0; i < bytes; ++i)
        d[i] = s[i];
}

void swap(char* x, char* y){
    char z = *x;
    *x = *y;
    *y = z;
}

void int_to_acsii(int n, char str[]){
    int i = 0, j = 0, sign;
    if((sign = n) < 0) n = -n;
    do{
        str[i++] = n % 10 + '0';
    }while((n /= 10) > 0);

    if(sign < 0) str[i++] = '-';
    str[i--] = '\0';
    while(j < i){
        swap(&str[j++], &str[i--]);
    }
}

