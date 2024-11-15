//
// Created by szymon on 31/08/2021.
//

#include "task.h"
#include "../libc/memory.h"

struct Task* current_task = 0;
extern uint32_t page_directory;
extern uint32_t heap_start;

void initialise_multitasking(){
    current_task = k_malloc(sizeof(struct Task));
    current_task->STATE = RUN;
    current_task->cr3 = page_directory;
    current_task->next_task = current_task;
    current_task->stack_top = (void*)heap_start;
}
