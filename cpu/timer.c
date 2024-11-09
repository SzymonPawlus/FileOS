//
// Created by szymon on 31/07/2021.
//

#include "timer.h"
#include "../drivers/ports.h"

void init_timer(uint32_t freq, isr_t handler){
    register_interrupt_handler(IRQ0, handler);

    uint32_t divisor = 1193180 / freq;
    uint8_t low  = (uint8_t)(divisor & 0xFF);
    uint8_t high = (uint8_t)( (divisor >> 8) & 0xFF);

    port_byte_out(0x43, 0x36);
    port_byte_out(0x40, low);
    port_byte_out(0x40, high);
}
