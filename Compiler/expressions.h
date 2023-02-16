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
#include <inttypes.h>

#include "map.h"
#include "slicec.h"
#include "interpreter.h"
#include "compiler.h"

// The plan is to honor as many C operators as possible with
// the same precedence and associativity
// e<n> implements operators with precedence 'n' (smaller is higher)

// () [] . -> ...

//idea is to push values onto the stack, and then pop them off in other methods when used
void ce1(Compiler* i, bool effects) {
    optionalSlice id = cconsumeIdentifier(i);
    if (id.valid) {
        if(!equalsString(id.value, "print") && cconsume(i, "(")) { //normal function
            callFunction(i, effects, id);
            puts("    push %rax");
        } else if (equalsString(id.value, "print") && cconsume(i, "(")) { //print function
            cexpression(i, effects);
            cconsume(i, ")");
            puts("    call print");
        } else {
            printf("    push %ld(%%rbp)\n", get(*(i->symbolTable), id.value).value); //get the variable at its memory location
        }
    } else {
        optionalInt v = cconsumeLiteral(i);
        if (v.valid) {
            printf("    mov $%ld, %%rdi\n", v.value);
            puts("    push %rdi");
        } else if (cconsume(i, "(")) {
            cexpression(i, effects);
            cconsume(i, ")");
        } else if (cconsume(i, "!")) {
            ce1(i, effects);
            puts("    pop %rdi");
            puts("    test %rdi, %rdi");
            puts("    setz %dil");
            puts("    and $1, %rdi");
            puts("    push %rdi");
        } else {
            cfail(i);
        }
    }
}

// * / % (Left)
void ce3(Compiler* i, bool effects) {
    ce1(i, effects);

    while (true) {
        if (cconsume(i, "*")) {
            ce1(i, effects);
            puts("    pop %rsi");
            puts("    pop %rdi");
            puts("    imul %rsi, %rdi");
            puts("    push %rdi");
        } else if (cconsume(i, "/")) {
            ce1(i, effects);
            puts("    pop %rdi");
            puts("    pop %rax");
            puts("    mov $0, %edx");
            puts("    div %rdi");
            puts("    push %rax");
        } else if (cconsume(i, "%")) {;
            ce1(i, effects);
            puts("    pop %rdi");
            puts("    pop %rax");
            puts("    mov $0, %edx");
            puts("    div %rdi");
            puts("    push %rdx");
        } else {
            return;
        }
    }
}

// (Left) + -
void ce4(Compiler* i, bool effects) {
    ce3(i, effects);

    while (true) {
        if (cconsume(i, "+")) {
            ce3(i, effects);
            puts("    pop %rsi");
            puts("    pop %rdi");
            puts("    add %rsi, %rdi");
            puts("    push %rdi");
        } else if (cconsume(i, "-")) {
            ce3(i, effects);
            puts("    pop %rsi");
            puts("    pop %rdi");
            puts("    sub %rsi, %rdi");
            puts("    push %rdi");
        } else {
            return;
        }
    }
}

// << >>
void ce5(Compiler* i, bool effects) {
    ce4(i, effects);
}

// < <= > >=
void ce6(Compiler* i, bool effects) {
    ce5(i, effects);
    
    while (true) {
        if (cconsume(i, "<=")) {
            ce5(i, effects);
            puts("    pop %rsi");
            puts("    pop %rdi");
            puts("    cmp %rdi, %rsi");
            puts("    setae %dil");
            puts("    and $1, %rdi");
            puts("    push %rdi");
        } else if (cconsume(i, ">=")) {
            ce5(i, effects);
            puts("    pop %rsi");
            puts("    pop %rdi");
            puts("    cmp %rdi, %rsi");
            puts("    setbe %dil");
            puts("    and $1, %rdi");
            puts("    push %rdi");
        } else if (cconsume(i, "<")) {
            ce5(i, effects);
            puts("    pop %rsi");
            puts("    pop %rdi");
            puts("    cmp %rdi, %rsi");
            puts("    seta %dil");
            puts("    and $1, %rdi");
            puts("    push %rdi");
        } else if (cconsume(i, ">")) {
            ce5(i, effects);
            puts("    pop %rsi");
            puts("    pop %rdi");
            puts("    cmp %rdi, %rsi");
            puts("    setb %dil");
            puts("    and $1, %rdi");
            puts("    push %rdi");
        } else {
            return;
        }
    }

}

// == !=
void ce7(Compiler* i, bool effects) {
    ce6(i, effects);

    while (true) {
        if (cconsume(i, "==")) {
            ce6(i, effects);
            puts("    pop %rsi");
            puts("    pop %rdi");
            puts("    cmp %rdi, %rsi");
            puts("    sete %dil");
            puts("    and $1, %rdi");
            puts("    push %rdi");
        } else if (cconsume(i, "!=")) {
            ce6(i, effects);
            puts("    pop %rsi");
            puts("    pop %rdi");
            puts("    cmp %rdi, %rsi");
            puts("    setne %dil");
            puts("    and $1, %rdi");
            puts("    push %rdi");
        } else {
            return;
        }
    }
}

// (left) &
void ce8(Compiler* i, bool effects) {
    ce7(i, effects);
}

// ^
void ce9(Compiler* i, bool effects) {
    ce8(i, effects);
}

// |
void ce10(Compiler* i, bool effects) {
    ce9(i, effects);
}

// &&
void ce11(Compiler* i, bool effects) {
    ce10(i, effects);

    while (true) {
        if (cconsume(i, "&&")) {
            ce10(i, effects);
            puts("    pop %rsi");
            puts("    pop %rdi");
            puts("    test %rsi, %rsi");
            puts("    setnz %sil");
            puts("    test %rdi, %rdi");
            puts("    setnz %dil");
            puts("    and %rsi, %rdi");
            puts("    and $1, %rdi");
            puts("    push %rdi");
        } else {
            return;
        }
    }
}

// ||
void ce12(Compiler* i, bool effects) {
    ce11(i, effects);

    while (true) {
        if (cconsume(i, "||")) {
            ce11(i, effects);
            puts("    pop %rsi");
            puts("    pop %rdi");
            puts("    mov $0, %eax");
            puts("    or %rsi, %rdi");
            puts("    setnz %al");
            puts("    push %rax");
        } else {
            return;
        }
    }
}

// (right with special treatment for middle cexpression) ?:
void ce13(Compiler* i, bool effects) {
    ce12(i, effects);
}

// = += -= ...
void ce14(Compiler* i, bool effects) {
    ce13(i, effects);
}

// ,
void ce15(Compiler* i, bool effects) {
    ce14(i, effects);
}

void cexpression(Compiler* i, bool effects) {
    char* start = i->current;
    //check if constant folding can be performed
    if(foldable(i, effects)) {
        Interpreter inter = createInterpreter(start);
        uint64_t val = expression(&inter, effects, false);
        printf("    mov $%ld, %%rdi\n", val);
        puts("    push %rdi");
        i->current = getCurrent(&inter);
        clearInterpreter(inter);
    }
    else {
        ce15(i, effects);
   }
}