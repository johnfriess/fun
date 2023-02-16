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

typedef struct {
    char* program;
    char* current;
    Map* symbolTable;
    Slice function;
    uint64_t params;
    uint64_t ifst;
    uint64_t whilest;
} Compiler;

void cfail(Compiler* i) {
    printf("failed at offset %ld\n",(size_t)(i->current - i->program));
    printf("%s\n",i->current);
    exit(1);
}

void cendOrFail(Compiler* i) {
    while (isspace(*(i->current))) {
        i->current += 1;
    }
    if (*(i->current) != 0) cfail(i);
}

void cskip(Compiler* i) {
    while (isspace(*(i->current))) {
        i->current += 1;
    }
}

bool cconsume(Compiler* inter, const char* str) {
    cskip(inter);

    size_t i = 0;
    while (true) {
      char const expected = str[i];
      char const found = inter->current[i];
      if (expected == 0) {
        /* survived to the end of the expected string */
        inter->current += i;
        return true;
      }
      if (expected != found) {
        return false;
      }
      // assertion: found != 0
      i += 1;
    }
}

void cconsumeOrFail(Compiler* i, const char* str) {
    if (!cconsume(i, str)) {
        cfail(i);
    }
}

void cskipBraces(Compiler* i, char* str1, char* str2) {
    uint64_t braces = 1;
    cconsumeOrFail(i, str1);
    while(braces != 0)  {
        if(cconsume(i, str1)) {
            braces++;
            continue;
        }
        else if(cconsume(i, str2)) {
            braces--;
            continue;
        }
        i->current++;
    }
}

optionalSlice cconsumeIdentifier(Compiler* i) {
    cskip(i);

    if (isalpha(*(i->current))) {
        char * start = i->current;
        do {
            i->current += 1;
        } while(isalnum(*(i->current)));
        optionalSlice slice = {true, createSliceLen(start, (size_t)(i->current - start))};
        return slice;
    } else {
        optionalSlice slice = {false, createSliceLen(i->current, (size_t)(0))};
        return slice;
    }
}

optionalInt cconsumeLiteral(Compiler* i) {
    cskip(i);
    if (isdigit(*(i->current))) {
        uint64_t v = 0;
        do {
        v = 10*v + ((*(i->current)) - '0');
        i->current += 1;
        } while (isdigit(*(i->current)));
        optionalInt n = {true, v};
        return n;
    } else {
        optionalInt n = {false, 0};
        return n;
    }
}

Compiler createCompiler(char* prog) {
    Compiler i = {prog, prog, createMap(), createSliceLen(prog, (size_t)(0))};
    return i;
}

void cexpression(Compiler* i, bool effects); 

void processFunction(Compiler* i, bool effects);

void callFunction(Compiler* i, bool effects, optionalSlice id);

void compileFunction(Compiler* i, bool effects);

void processIf(Compiler* i, bool effects);

void processWhile(Compiler* i, bool effects);

void processReturn(Compiler* i, bool effects);

bool foldable(Compiler* i, bool effects) {
    char* start = i->current;
    while(*(i->current) != '\n' && *(i->current) != EOF) {
        if(isalpha(*(i->current))) {
            i->current = start;
            return false;
        }
        i->current++;
    }
    return true;
}

#include "expressions.h"

bool cstatement(Compiler* i, bool effects) {
    optionalSlice id = cconsumeIdentifier(i);
    if(id.valid) {
        if (equalsString(id.value, "print") && cconsume(i, "(")) {
            // print ...
            i->current--;
            cexpression(i, effects);
            cconsume(i, ")");
            puts("    call print");
            puts("    pop %rsi");
            return true;
        } else if(equalsString(id.value, "if")) {
            processIf(i, effects);
            return true;
        } else if(equalsString(id.value, "while")) {
            processWhile(i, effects);
            return true;
        } else if(equalsString(id.value, "fun")) {
            processFunction(i, effects);
            return true;
        } else if (equalsString(id.value, "return")) {
            processReturn(i, effects);
            return true;
        } else if (cconsume(i, "=")) {
            cexpression(i, effects);
            printf("    pop %ld(%%rbp)\n", get(*(i->symbolTable), id.value).value);
            return true;
        } else if (cconsume(i, "(")) {
            callFunction(i, effects, id);
            return true;
        } else {
            cfail(i);
        }
    }
    return false;
}

void cstatements(Compiler* i, bool effects) {
    while (cstatement(i, effects));
}

void crun(Compiler* i) {
    cstatements(i, true);
    cendOrFail(i);
};

void clearCompiler(Compiler i) {
    clearMap(*(i.symbolTable));
    free(i.symbolTable);
}

void processFunction(Compiler* i, bool effects) {
    optionalSlice name = cconsumeIdentifier(i);
    if(name.valid) {
        //function label + prologue
        printf("._");
        print(name.value);
        puts(":");
        puts("    push %rbp");
        puts("    mov %rsp, %rbp");

        //function start label without prologue (for tail recursion optimization)
        printf("._");
        print(name.value);
        puts("._start:");

        //count params
        uint64_t n = 0;
        cconsumeOrFail(i, "(");
        char* start = i->current;
        while(!cconsume(i, ")")) {
            if(n != 0) cconsumeOrFail(i, ",");
            cconsumeIdentifier(i);
            n++;
        }
        i->params = n;
        char* end = i->current;

        //put the value of the parameters on the stack
        i->current = start;
        Map* localMap = createMap();
        for(int64_t p = n-1; p >= 0; p--) {
            if(p != n-1) cconsumeOrFail(i, ",");
            optionalSlice identifier = cconsumeIdentifier(i);
            put(localMap, identifier.value, 8*(p+2));
        }
        i->current = end;

        //determine the local variables that are used and allocate the appropriate memory on the stack
        uint64_t braces = 1;
        cconsumeOrFail(i, "{");
        int64_t localVariables = 0;
        while(braces != 0) {
            optionalSlice id = cconsumeIdentifier(i);
            if(id.valid && cconsume(i, "=") && !contains(localMap, id.value)) {
                put(localMap, id.value, -8*(++localVariables));
            }

            if(cconsume(i, "{")) {
                braces++;
                continue;
            }
            else if(cconsume(i, "}")) {
                braces--;
                continue;
            }
            
            if(!id.valid)
                i->current++;
        }
        printf("    sub $%ld, %%rsp\n", localVariables*8);

        //set variables used later
        Map* currentTable = i->symbolTable;
        i->symbolTable = localMap;
        i->current = end;
        i->function = name.value;
        
        //process the function
        compileFunction(i, effects);

        //reset old table
        i->symbolTable = currentTable;

        //free memory no longer used
        clearMap(*localMap);
        free(localMap);
    }
    else {
        cfail(i);
    }

}

void processIf(Compiler* i, bool effects) {
    //keep track of if statement level
    i->ifst++;
    uint64_t localIfst = i->ifst;
    cexpression(i, effects);
    puts("    pop %rdi");
    puts("    test %rdi, %rdi");
    puts("    setnz %dil");
    puts("    and $1, %rdi");

    //jump to start if statement the zero flag is not true
    printf("    jnz ._");
    print(i->function);
    printf("._if%ld", localIfst);
    printf("._true\n");

    //jump to start if statement the zero flag is true
    printf("    jz ._");
    print(i->function);
    printf("._if%ld", localIfst);
    printf("._false\n");

    //end
    printf("    jmp ._");
    print(i->function);
    printf("._if%ld", localIfst);
    printf("._end\n");

    //if statement true label
    printf("._");
    print(i->function);
    printf("._if%ld", localIfst);
    printf("._true:\n");
    uint64_t braces = 1;
    cconsumeOrFail(i, "{");
    while(braces != 0) {
        cstatement(i, effects);

        if(cconsume(i, "{"))
            braces++;
        if(cconsume(i, "}"))
            braces--;
    }

    //jump to end of if (don't process else if the condition is true)
    printf("    jmp ._");
    print(i->function);
    printf("._if%ld", localIfst);
    printf("._end\n");

    //else condition label
    printf("._");
    print(i->function);
    printf("._if%ld", localIfst);
    printf("._false:\n");
    char* start = i->current;
    optionalSlice nextId = cconsumeIdentifier(i);
    if(equalsString(nextId.value, "else")) {
        braces = 1;
        cconsumeOrFail(i, "{");
        while(braces != 0) {
            cstatement(i, effects);

            if(cconsume(i, "{"))
                braces++;
            if(cconsume(i, "}"))
                braces--;                 
        }
    }
    else {
        i->current = start;
    }

    //end label
    printf("._");
    print(i->function);
    printf("._if%ld", localIfst);
    printf("._end:\n");
}

void processWhile(Compiler* i, bool effects) {
    //keep track of while loop level
    i->whilest++;
    uint64_t localWhilest = i->whilest;
    printf("._");
    print(i->function);
    printf("._while%ld._start:\n", localWhilest);
    cexpression(i, effects);

    //check if condition is true
    puts("    pop %rdi");
    puts("    test %rdi, %rdi");
    puts("    setnz %dil");
    puts("    and $1, %rdi");

    //jump to end of while loop if zero flag is true
    printf("    jz ._");
    print(i->function);
    printf("._while%ld._end\n", localWhilest);
    cconsumeOrFail(i, "{");
    while(!cconsume(i, "}")) {
        cstatement(i, effects);
    }
    
    //at the end, always jump to the start of the function
    printf("    jmp ._");
    print(i->function);
    printf("._while%ld._start\n", localWhilest);
    printf("._");
    print(i->function);
    printf("._while%ld._end:\n", localWhilest);
}

void processReturn(Compiler* i, bool effects) {
    //check if return is a lone function (can use tail recursion optimization)
    bool onlyFunction = false;
    char* estart = i->current;
    char* start = i->current;
    optionalSlice v = cconsumeIdentifier(i);
    if(equalsSlice(v.value, i->function) && !contains(i->symbolTable, v.value)) {
        onlyFunction = true;
        start = i->current;
        cskipBraces(i, "(", ")");
        while(*(i->current) != '\n') {
            if(!isspace(*(i->current))) {
                onlyFunction = false;
            }
            i->current++;
        }
    }

    //tail recursion optimization
    if(onlyFunction) {
        i->current = start;
        cconsumeOrFail(i, "(");
        for(int64_t p = i->params-1; p >= 0; p--) {
            if(p != i->params-1) cconsumeOrFail(i, ",");
            cexpression(i, effects);
            printf("    pop %ld(%%rbp)\n", 8*(p+2));
        }
        cconsumeOrFail(i, ")");
        printf("    jmp ._");
        print(i->function);
        puts("._start");
    }
    else {
        i->current = estart;
        cexpression(i, effects);
        puts("    pop %rax");
        puts("    mov %rbp, %rsp");
        puts("    pop %rbp");
        puts("    ret");
    }
}

void callFunction(Compiler* i, bool effects, optionalSlice id) {
    //go through the parameters
    uint64_t n = 0;
    while(!cconsume(i, ")")) {
        if(n != 0) cconsumeOrFail(i, ",");
        cexpression(i, effects);
        n++;
    }

    //call function
    printf("    call ._");
    print(id.value);
    printf("\n");

    //clear local variables from the top of the stack
    printf("    add $%ld, %%rsp\n", 8*n);
}

void compileFunction(Compiler* i, bool effects) {
    //process the function statements
    cconsumeOrFail(i, "{");
    while(!cconsume(i, "}")) {
        cstatement(i, effects);
    }

    //epilogue
    puts("    mov %rbp, %rsp");
    puts("    pop %rbp");
    puts("    mov $0, %rax");
    puts("    ret");
}