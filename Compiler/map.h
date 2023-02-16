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

//optional macro for int and slice
#define optional(type) struct { bool valid; type value;}

typedef optional(int64_t) optionalInt;
typedef optional(Slice) optionalSlice;

//linked list (for chaining)
typedef struct Node {
    Slice key;
    int64_t value;
    struct Node* next;
} Node;

//map
typedef struct {
    Node** map;
    uint64_t size;
    uint64_t capacity;
} Map;

//initialize values in map
Map* createMap() {
    Map* m = (Map*)malloc(sizeof(Map));
    m->size = 0;
    m->capacity = 1;
    m->map = (Node**)malloc(sizeof(Node*) * m->capacity);
    for (size_t i = 0; i < m->capacity; i++) {
        m->map[i] = NULL;
    }
    return m;
}

optionalInt get(Map m, Slice s) {
    //find hashing index and traverse through the index's linked list until found
    size_t index = hash(s) % m.capacity;
    Node* n = m.map[index];
    optionalInt v = {false, 0};

    while(n != NULL) {
        if(equalsSlice(s, n->key)) {
            v.valid = true;
            v.value = n->value;
            return v;
        }
        n = n->next;
    }
    return v;
}

void resize(Map* m) {
    //create new mapping with updated capacity
    uint64_t updatedCapacity = 2*m->capacity;
    Node** n = (Node**)malloc(sizeof(Node*) * updatedCapacity);
    for (size_t i = 0; i < updatedCapacity; i++) {
        n[i] = NULL;
    }
    

    //rehash all of the current instances in the map
    for(size_t i = 0; i < m->capacity; i++) { 
        Node* c = m->map[i];
        while(c != NULL) {
            Node* next = c->next;
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

void put(Map* m, Slice s, int64_t v) {
    //check if index already contains key and update if it does
    size_t index = hash(s) % m->capacity;
    Node* n = m->map[index];
    while(n != NULL) {
        if(equalsSlice(s, n->key)) {
            n->value = v;
            return;
        }
        n = n->next;
    }

    //add node to front of linked list
    n = (Node*)malloc(sizeof(Node));
    n->key = s;
    n->value = v;
    n->next = m->map[index];
    m->map[index] = n;
    m->size++;

    //resize if necessary
    if(m->size >= m->capacity)
        resize(m);
}

bool contains(Map* m, Slice s) {
    size_t index = hash(s) % m->capacity;
    Node* n = m->map[index];
    while(n != NULL) {
        if(equalsSlice(s, n->key)) {
            return true;
        }
        n = n->next;
    }
    return false;
}

//recursivley free linked list
void clearLinkedList(Node* n) {
    if(n != NULL) {
        clearLinkedList(n->next);
        free(n);
    }
}

//go through all linked lists and free them
void clearMap(Map m) {
    for(size_t i = 0; i < m.capacity; i++) {
        clearLinkedList(m.map[i]);
    }
    free(m.map);
}

//print map, useful for debugging
void printMap(Map m) {
    for(size_t i = 0; i < m.capacity; i++) { 
        Node* c = m.map[i];
        if(c != NULL) {
            printf("\nkey: ");
            print(c->key);
            printf("\tvalue: %ld", c->value);
        }
    }
}