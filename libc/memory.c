//
// Created by szymon on 02/08/2021.
//

#include "memory.h"
#include "../kernel/util.h"

extern unsigned int __HEAP;
extern unsigned int __HEAP_END;

const void* heap_start = &__HEAP;
const void* heap_end   = &__HEAP_END;



// Bump allocation
/*void* current = &__HEAP;
int allocations = 0;

void* k_malloc(uint32_t size) {
    if(current + size <= heap_end){
        void* pointer = current;
        current += size;
        allocations++;
        return pointer;
    }else{
        return 0;
    }
}

void k_free(void* pointer) {
    if(!--allocations) current = &__HEAP;
}*/

// Linked list allocator

struct BlockNode {
    uint32_t              size;
    struct BlockNode* next;
};

int is_contiguous(struct BlockNode* l, struct BlockNode* r);
struct BlockNode* merge(struct BlockNode* l, struct BlockNode* r);

struct BlockNode* head = (struct BlockNode*)&__HEAP;

void k_malloc_init(){
    head->size = (uint32_t) (heap_end - heap_start - sizeof(struct BlockNode));
    head->next = 0;
};

void* k_malloc(uint32_t size) {
    // Look for the node which includes enough space
    struct BlockNode* node = head;
    while(node->size < (size + sizeof(struct BlockNode))){
        node = node->next;
        if(!node) return 0; // Return NULL if not found
    }

    // TODO - handle case: node->size < size
    // If found then resize current block and create new allocated block
    node->size -= (size + sizeof(struct BlockNode));
    struct BlockNode* newNode = (struct BlockNode*)((void*)node + node->size + sizeof(struct BlockNode));
    newNode->size = size;
    newNode->next = 0;

    return newNode + 1;
}

void k_free(void* pointer) {
    struct BlockNode* to_free = pointer - sizeof(struct BlockNode);
    struct BlockNode* node = head;
    struct BlockNode* previous = 0;

    while(node < to_free && node) {
        previous = node;
        node = node->next;
    }

    previous->next = to_free;
    to_free->next = node;
    if(is_contiguous(previous, to_free)) to_free = merge(previous, to_free);
    if(is_contiguous(to_free, node)) merge(to_free, node);


}

void* k_realloc(void *p, uint32_t size){
    if(!p)
        return k_malloc(size);
    void* np = k_malloc(size);
    if(!np)
        return 0;
    struct BlockNode* to_free = p - sizeof(struct BlockNode);
    k_memcpy(p, np, to_free->size);
    k_free(p);
    return np;
}

// Utilities
void k_memset(void* memory, uint32_t size, uint8_t value){
    uint8_t* temp = (uint8_t*) memory;
    for (; size > 0; size--)
        *temp++ = value;
}

// ! Direction Matters !
// Says whether two nodes are contiguous on the heap
int is_contiguous(struct BlockNode* l, struct BlockNode* r){
    return ((void*)(l + 1) + l->size) == r;
}

struct BlockNode* merge(struct BlockNode* l, struct BlockNode* r){
    l->next = r->next;
    l->size += r->size + sizeof(struct BlockNode);
    // Not necessary but want to see it in debugger
    r->size = 0;
    r->next = 0;
    return l;
}

int k_memcmp(void* mem1, void* mem2, uint32_t size){
    uint8_t* memcp1 = mem1;
    uint8_t* memcp2 = mem2;
    for (uint32_t i = 0; i < size; ++i){
        if(*memcp1 != *memcp2) return 1;
        memcp1++; memcp2++;
    }
    return 0;
}

void k_memreplace(uint8_t* mem, char to_replace, char to_replace_with, uint32_t size){
    for (int i = 0; i < size; ++i)
        if(mem[i] == to_replace) mem[i] = to_replace_with;
}




