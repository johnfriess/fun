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

typedef struct {
    char* start;
    size_t len;
} Slice;

//constructor using length
Slice createSliceLen(char* s, size_t l) {
    Slice slice = {s, l};
    return slice;
}

//constructor using end character
Slice createSliceEnd(char* s, char* e) {
    Slice slice = {s, e - s};
    return slice;
}

bool equalsString(Slice s, char* p) {
    for (size_t i = 0; i < s.len; i++) {
      if (p[i] != s.start[i])
        return false;
    }
    return p[s.len] == 0;
}

bool equalsSlice(Slice s1, Slice s2) {
  if (s1.len != s2.len)
    return false;
  for (size_t i = 0; i < s1.len; i++) {
    if (s2.start[i] != s1.start[i])
      return false;
  }
  return true;
}

bool isIdentifier(Slice s) {
  if (s.len == 0)
      return 0;
  if (!isalpha(s.start[0]))
      return false;
  for (size_t i = 1; i < s.len; i++)
      if (!isalnum(s.start[i]))
        return false;
  return true;
}

void print(Slice s) {
  for (size_t i = 0; i < s.len; i++) {
      printf("%c", s.start[i]);
  }
}

size_t hash(Slice s) {
  size_t out = 5381;
  for (size_t i = 0; i < s.len; i++) {
    char const c = s.start[i];
    out = out * 33 + c;
  }
  return out;
}