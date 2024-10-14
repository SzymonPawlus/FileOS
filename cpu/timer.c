//
// Created by szymon on 31/07/2021.
//

#include "timer.h"
#include "../drivers/ports.h"

void init_timer(u32 freq, isr_t handler){
    register_interrupt_handler(IRQ0, handler);

    u32 divisor = 1193180 / freq;
    u8 low  = (u8)(divisor & 0xFF);
    u8 high = (u8)( (divisor >> 8) & 0xFF);

    port_byte_out(0x43, 0x36);
    port_byte_out(0x40, low);
    port_byte_out(0x40, high);
}
