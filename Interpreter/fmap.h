#pragma once

#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "slicec.h"
#include "map.h"

//storing function information
typedef struct {
    char* start;
    uint64_t numParams;
    Slice* parameters;
} Function;

//linked list (for chaining)
typedef struct NodeF {
    Slice key;
    Function fun;
    struct NodeF* next;
} NodeF;

//map
typedef struct {
    NodeF** map;
    uint64_t size;
    uint64_t capacity;
} MapF;

//initialize values in map
MapF* createMapF() {
    MapF* m = (MapF*)malloc(sizeof(MapF));
    m->size = 0;
    m->capacity = 1;
    m->map = (NodeF**)malloc(sizeof(NodeF*) * m->capacity);
    for (size_t i = 0; i < m->capacity; i++) {
        m->map[i] = NULL;
    }
    return m;
}

Function getF(MapF m, Slice s) {
    //find hashing index and traverse through the index's linked list until found
    size_t index = hash(s) % m.capacity;
    NodeF* n = m.map[index];
    
    while(n != NULL) {
        if(equalsSlice(s, n->key))
            return n->fun;
        n = n->next;
    }

    Function f = {NULL, 0, NULL};
    return f;
}

void resizeF(MapF* m) {
    //create new mapping with updated capacity
    uint64_t updatedCapacity = 2*m->capacity;
    NodeF** n = (NodeF**)malloc(sizeof(NodeF*) * updatedCapacity);
    for (size_t i = 0; i < updatedCapacity; i++) {
        n[i] = NULL;
    }
    
    //rehash all of the current instances in the map
    for(size_t i = 0; i < m->capacity; i++) { 
        NodeF* c = m->map[i];
        while(c != NULL) {
            NodeF* next = c->next;
            size_t index = hash(c->key) % updatedCapacity;
            c->next = n[index];
            n[index] = c;
            c = next;
        }
    }

    free(m->map);
    m->map = n;
    m->capacity = updatedCapacity;
}

void putF(MapF* m, Slice s, Function f) {
    //check if index already contains key and update if it does
    size_t index = hash(s) % m->capacity;
    NodeF* n = m->map[index];
    while(n != NULL) {
        if(equalsSlice(s, n->key)) {
            n->fun = f;
            return;
        }
        n = n->next;
    }

    //add node to front of linked list
    n = (NodeF*)malloc(sizeof(NodeF));
    n->key = s;
    n->fun = f;
    n->next = m->map[index];
    m->map[index] = n;
    m->size++;

    //resize if necessary
    if(m->size >= m->capacity)
        resizeF(m);
}

//recursivley free linked list
void clearLinkedListF(NodeF* n) {
    if(n != NULL) {
        free(n->fun.parameters);
        clearLinkedListF(n->next);
        free(n);
    }
}

//go through all linked lists and free them
void clearMapF(MapF m) {
    for(size_t i = 0; i < m.capacity; i++) {
        clearLinkedListF(m.map[i]);
    }
    free(m.map);
}