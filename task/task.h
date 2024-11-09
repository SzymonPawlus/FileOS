//
// Created by szymon on 31/08/2021.
//

#ifndef FILEOS_TASK_H
#define FILEOS_TASK_H

#include "../cpu/types.h"

struct Task {
    void* stack_top;
    uint32_t cr3;
    struct Task* next_task; // for linked list
    enum  {
        BLOCK,
        WAIT_FOR_RUN,
        RUN
    } STATE;
};

void initialise_multitasking();

#endif //FILEOS_TASK_H
