//
// Created by szymon on 31/07/2021.
//

#ifndef FILEOS_KEYBOARD_H
#define FILEOS_KEYBOARD_H

#include "../cpu/types.h"
#include "../cpu/isr.h"

void init_keyboard(isr_t callback);
int read_key_buffer(char* buffer);

#endif //FILEOS_KEYBOARD_H
