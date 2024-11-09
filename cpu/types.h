//
// Created by szymon on 31/07/2021.
//

#ifndef FILEOS_TYPES_H
#define FILEOS_TYPES_H

#include <stdint.h>

#define low_16(address) (uint16_t)((address) & 0xFFFF)
#define high_16(address) (uint16_t)(((address) >> 16) & 0xFFFF)


#endif //FILEOS_TYPES_H
