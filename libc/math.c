//
// Created by szymon on 15/08/2021.
//

#include "math.h"

u32 max(u32 a, u32 b){
    if(a > b) return a;
    else return b;

}

u32 min(u32 a, u32  b){
    if(a < b) return a;
    else return b;
}

u32 clamp(u32 val, u32 a, u32 b){
    return max(a, min(val, b));
}