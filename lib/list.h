// list.h - create a list of any kind in C
// append, pop, safely retrieve
// licenced to you under the MIT license
// example: 
//
// #include "list.h"
// 
// UseList(int);
// 
// int main() {
//     List(int) myList = newList(int);
//     append(myList, 32);
//     printf("%d\n", *get(myList, 0));
//     pop(myList);
//     destroy(myList);
// }
// 
#ifndef LIST_H
#define LIST_H

#include <stdlib.h>
#include <stdio.h>

#define LIST_H_INIT_CAPACITY 32

#define UseList(type) struct __List_##type { type* pointer; size_t count; size_t capacity; size_t itemsize; }
#define List(type) struct __List_##type

#define newList(type) ({ \
    List(type) __list_tmp = { .pointer = malloc(sizeof(type) * LIST_H_INIT_CAPACITY), .count = 0, .capacity = LIST_H_INIT_CAPACITY, .itemsize = sizeof(type) };\
    if (__list_tmp.pointer == NULL) { \
        printf("list.h:32 (newList(type)) - failed to allocate memory"); exit(1);\
    }\
    __list_tmp;\
})

#define append(list, item) {\
    if (list.pointer == NULL) {\
        printf("list.h:39 (append(list, item)) - list pointer is null (perhaps you accidently destroyed the list?)"); exit(1);\
    }\
    if (list.capacity < list.count + 1) {\
        list.capacity *= 2;\
        void* __tmp_ptr = realloc(list.pointer, list.capacity * list.itemsize);\
        if (__tmp_ptr == NULL) {\
            printf("list.h:45 (append(list, item)) - failed to allocate memory"); exit(1);\
        }\
        list.pointer = __tmp_ptr;\
    }\
    list.pointer[list.count] = item;\
    list.count++;\
}

#define get(list, idx) ({\
    if (list.pointer == NULL) {\
        printf("list.h:55 (get(list, idx)) - list pointer is null (perhaps you accidently destroyed the list?)"); exit(1);\
    }\
    (idx >= list.count) ? NULL : &list.pointer[idx];\
})

#define pop(list) {\
    if (list.pointer == NULL) {\
        printf("list.h:65 (pop(list)) - list pointer is null (perhaps you accidently destroyed the list?)"); exit(1);\
    }\
    if (list.count > 0) list.count--;\
}

#define destroy(list) if (list.pointer != NULL) {\
    free(list.pointer); list.pointer = NULL; \
}

#endif