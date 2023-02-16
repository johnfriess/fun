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
#include "compiler.h"

int main() {
    // reads the fun program from stdin
    uint64_t inputLen = 10000;
    char* prog = (char*)(calloc(sizeof(char), inputLen));
    char c;
    uint64_t index = 0;
    bool comment = false;
    while ((c = getchar()) != EOF) {
        if(c == '#')
            comment = true;

        if(!comment)
            prog[index++] = c;

        if(comment && c == '\n')
            comment = false;
    }
    
    puts("    .data");
    puts("format: .byte '%', 'l', 'u', 10, 0");
    puts("    .text");
    puts("    .global main");
    puts("print:");
    puts("    push %rbp");
    puts("    mov %rsp, %rbp");
    puts("    mov $0, %rax");
    puts("    lea format(%rip), %rdi");
    puts("    mov 16(%rsp), %rsi");
    puts("    .extern printf");
    puts("    and $0xFFFFFFFFFFFFFFF0, %rsp");
    puts("    call printf");
    puts("    mov $0, %rax");
    puts("    mov %rbp, %rsp");
    puts("    pop %rbp");
    puts("    ret");
    puts("main:");
    puts("    call ._main");
    puts("    ret");

    Compiler i = createCompiler(prog);
    crun(&i);

    clearCompiler(i);
    free(prog);
    return 0;
}
