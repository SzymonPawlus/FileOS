//
// Created by szymon on 31/07/2021.
//

#ifndef FILEOS_TIMER_H
#define FILEOS_TIMER_H

#include "../kernel/util.h"
#include "isr.h"

void init_timer(uint32_t freq, isr_t handler);

#endif //FILEOS_TIMER_H
