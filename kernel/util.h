//
// Created by szymon on 31/07/2021.
//

#ifndef FILEOS_UTIL_H
#define FILEOS_UTIL_H

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) > (y)) ? (y) : (x))
void k_memcpy(void *source, void *dest, int bytes);
void int_to_acsii(int n, char str[]);

#endif //FILEOS_UTIL_H
