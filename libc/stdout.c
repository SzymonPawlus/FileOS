//
// Created by szymon on 30/08/2021.
//

#include "stdout.h"
#include "../drivers/screen.h"
#include <stdarg.h>

void print_int(int number){
    int div = 1;
    while(number / div) div *= 10;
    while(div /= 10){
        int to_print = (number / div) % 10;
        kprint_char('0' + to_print);
    }
}

void printf(char* format, ...){
    va_list args;
    va_start(args, format);
    int was_percent = 0;
    while(*format){
        if(!was_percent){
            if(*format == '%')
                was_percent = 1;
            else
                kprint_char(*format);
        }
        else{
            if(*format == '%')
                kprint_char('%');
            else if(*format == 's'){
                char* message = va_arg(args, char*);
                kprint(message);
            }
            else if(*format == 'd'){
                int arg = va_arg(args, int);
                print_int(arg);
            }
            was_percent = 0;
        }
        format++;
    }
}
