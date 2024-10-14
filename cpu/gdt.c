//
// Created by szymon on 01/08/2021.
//

#include "gdt.h"

extern void gdt_setup();

void gdt_install(){
    gdt_setup();
}

void gdt_something(){
}
