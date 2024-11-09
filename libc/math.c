//
// Created by szymon on 15/08/2021.
//

#include "math.h"

uint32_t max(uint32_t a, uint32_t b){
    if(a > b) return a;
    else return b;

}

uint32_t min(uint32_t a, uint32_t  b){
    if(a < b) return a;
    else return b;
}

uint32_t clamp(uint32_t val, uint32_t a, uint32_t b){
    return max(a, min(val, b));
}