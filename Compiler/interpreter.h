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
#include "fmap.h"

typedef struct {
    char* program;
    char* current;
    Map* symbolTable;
    Map* localTable;
    MapF* functionTable;
    optionalInt ret;
} Interpreter;

char* getCurrent(Interpreter* i) {
    return i->current;
}

void fail(Interpreter* i) {
    printf("failed at offset %ld\n",(size_t)(i->current - i->program));
    printf("%s\n",i->current);
    exit(1);
}

void endOrFail(Interpreter* i) {
    while (isspace(*(i->current))) {
        i->current += 1;
    }
    if (*(i->current) != 0) fail(i);
}

void skip(Interpreter* i) {
    while (isspace(*(i->current))) {
        i->current += 1;
    }
}

bool consume(Interpreter* inter, const char* str) {
    skip(inter);

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

void consumeOrFail(Interpreter* i, const char* str) {
    if (!consume(i, str)) {
        fail(i);
    }
}

//skip until it reaches a corresponding end brace.
void skipBraces(Interpreter* i) {
    uint64_t braces = 1;
    consumeOrFail(i, "{");
    while(braces != 0)  {
        if(consume(i, "{")) {
            braces++;
            continue;
        }
        else if(consume(i, "}")) {
            braces--;
            continue;
        }
        i->current++;
    }
}

optionalSlice consumeIdentifier(Interpreter* i) {
    skip(i);

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

optionalInt consumeLiteral(Interpreter* i) {
    skip(i);
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

Interpreter createInterpreter(char* prog) {
    Interpreter i = {prog, prog, createMap(), createMap(), createMapF(), {false, 0}};
    return i;
}

uint64_t expression(Interpreter* i, bool effects, bool inFunction); 

uint64_t runFunction(Interpreter* i, bool effects, bool inFunction, optionalSlice id);

// The plan is to honor as many C operators as possible with
// the same precedence and associativity
// e<n> implements operators with precedence 'n' (smaller is higher)

// () [] . -> ...
uint64_t e1(Interpreter* i, bool effects, bool inFunction) {
    optionalSlice id = consumeIdentifier(i);
    if (id.valid) {
        if(!equalsString(id.value, "print") && consume(i, "(")) { //normal function
            return runFunction(i, effects, inFunction, id);
        } else if (equalsString(id.value, "print") && consume(i, "(")) { //print function
            uint64_t v = expression(i, effects, inFunction);
            consume(i, ")");
            printf("%"PRIu64"\n",v);
            return 0;
        } else {
            optionalInt global = get(*(i->symbolTable), id.value);
            if(inFunction) {
                optionalInt local = get(*(i->localTable), id.value);
                if(local.valid) {
                    return local.value;
                }
            }
            return global.value;
        }
    } else {
        optionalInt v = consumeLiteral(i);
        if (v.valid) {
            return v.value;
        } else if (consume(i, "(")) {
            uint64_t val = expression(i, effects, inFunction);
            consume(i, ")");
            return val;
        } else {
            fail(i);
            return 1;
        }
    }
}

// ++ -- unary+ unary- ... (Right)
uint64_t e2(Interpreter* i, bool effects, bool inFunction) {
    bool negation = false;
    bool negated = false;
    while (true) {
        if (consume(i, "!")) {
            negation = !negation;
            negated = true;
        } else {
            uint64_t v = e1(i, effects, inFunction);
            if(negation) {
                if(negated && v == 0)
                    return 1;
                return !v;
            } else {
                if(negated && v != 0)
                    return 1;
                return v;
            }
        }
    }
}

// * / % (Left)
uint64_t e3(Interpreter* i, bool effects, bool inFunction) {
    uint64_t v = e2(i, effects, inFunction);

    while (true) {
        if (consume(i, "*")) {
            v = v * e2(i, effects, inFunction);
        } else if (consume(i, "/")) {
            uint64_t right = e2(i, effects, inFunction);
            v = (right == 0) ? 0 : v / right;
        } else if (consume(i, "%")) {
            uint64_t right = e2(i, effects, inFunction);
            v = (right == 0) ? 0 : v % right;
        } else {
            return v;
        }
    }
}

// (Left) + -
uint64_t e4(Interpreter* i, bool effects, bool inFunction) {
    uint64_t v = e3(i, effects, inFunction);

    while (true) {
        if (consume(i, "+")) {
            v = v + e3(i, effects, inFunction);
        } else if (consume(i, "-")) {
            v = v - e3(i, effects, inFunction);
        } else {
            return v;
        }
    }
}

// << >>
uint64_t e5(Interpreter* i, bool effects, bool inFunction) {
    return e4(i, effects, inFunction);
}

// < <= > >=
uint64_t e6(Interpreter* i, bool effects, bool inFunction) {
    uint64_t v = e5(i, effects, inFunction);
    
    while (true) {
        if (consume(i, "<=")) {
            v = v <= e5(i, effects, inFunction);
        } else if (consume(i, ">=")) {
            v = v >= e5(i, effects, inFunction);
        } else if (consume(i, "<")) {
            v = v < e5(i, effects, inFunction);
        } else if (consume(i, ">")) {
            v = v > e5(i, effects, inFunction);
        } else {
            return v;
        }
    }

}

// == !=
uint64_t e7(Interpreter* i, bool effects, bool inFunction) {
    uint64_t v = e6(i, effects, inFunction);

    while (true) {
        if (consume(i, "==")) {
            v = e6(i, effects, inFunction) == v;
        } else if (consume(i, "!=")) {
            v = e6(i, effects, inFunction) != v;
        } else {
            return v;
        }
    }
}

// (left) &
uint64_t e8(Interpreter* i, bool effects, bool inFunction) {
    return e7(i, effects, inFunction);
}

// ^
uint64_t e9(Interpreter* i, bool effects, bool inFunction) {
    return e8(i, effects, inFunction);
}

// |
uint64_t e10(Interpreter* i, bool effects, bool inFunction) {
    return e9(i, effects, inFunction);
}

// &&
uint64_t e11(Interpreter* i, bool effects, bool inFunction) {
    uint64_t v = e10(i, effects, inFunction);

    while (true) {
        if (consume(i, "&&")) {
            v = e10(i, effects, inFunction) && v;
        } else {
            return v;
        }
    }
}

// ||
uint64_t e12(Interpreter* i, bool effects, bool inFunction) {
    uint64_t v = e11(i, effects, inFunction);

    while (true) {
        if (consume(i, "||")) {
            v = e11(i, effects, inFunction) || v;
        } else {
            return v;
        }
    }
}

// (right with special treatment for middle expression) ?:
uint64_t e13(Interpreter* i, bool effects, bool inFunction) {
    return e12(i, effects, inFunction);
}

// = += -= ...
uint64_t e14(Interpreter* i, bool effects, bool inFunction) {
    return e13(i, effects, inFunction);
}

// ,
uint64_t e15(Interpreter* i, bool effects, bool inFunction) {
    return e14(i, effects, inFunction);
}

uint64_t expression(Interpreter* i, bool effects, bool inFunction) {
    return e15(i, effects, inFunction);
}

bool statement(Interpreter* i, bool effects, bool inFunction) {
    optionalSlice id = consumeIdentifier(i);
    if(id.valid) {
        if (equalsString(id.value, "print") && consume(i, "(")) {
            // print ...
            i->current--;
            uint64_t v = expression(i, effects, inFunction);
            if (effects) {
                printf("%"PRIu64"\n",v);
            }
            return true;
        } else if(equalsString(id.value, "if")) {
            uint64_t v = expression(i, effects, inFunction);
            //if condition not met
            if(v == 0) {
                skipBraces(i);
                uint64_t braces = 1;
                char* start = i->current;
                optionalSlice nextId = consumeIdentifier(i);
                if(equalsString(nextId.value, "else")) {
                    braces = 1;
                    consumeOrFail(i, "{");
                    //call statement until equivalent end brace is reached or indicated that a value is returned
                    while(braces != 0) {
                        statement(i, effects, inFunction);
                        if(i->ret.valid)
                            return true;

                        if(consume(i, "{"))
                            braces++;
                        if(consume(i, "}"))
                            braces--;                 
                    }
                }
                else {
                    i->current = start;
                }
            }
            else { //if condition met
                uint64_t braces = 1;
                consumeOrFail(i, "{");
                //call statement until equivalent end brace is reached or indicated that a value is returned
                while(braces != 0) {
                    statement(i, effects, inFunction);
                    if(i->ret.valid)
                        return true;

                    if(consume(i, "{"))
                        braces++;
                    if(consume(i, "}"))
                        braces--;
                }

                char* start = i->current;
                optionalSlice nextId = consumeIdentifier(i);

                if(equalsString(nextId.value, "else")) { 
                    skipBraces(i);
                }
                else {
                    i->current = start;
                }
            }
            return true;
        } else if(equalsString(id.value, "while")) {
            char* start = i->current;
            uint64_t v = expression(i, effects, inFunction);
            if(v) {
                while(v) {
                    consumeOrFail(i, "{");
                    //call statement until end brace reached or indicated that a value is returned
                    while(!consume(i, "}")) {
                        statement(i, effects, inFunction);

                        if(i->ret.valid)
                            return true;
                    }
                    //keep track of the current location in case we don't need to go back
                    char* current = i->current;

                    //calculate result for current condition
                    i->current = start;
                    v = expression(i, effects, inFunction);
                    
                    //go back if while condition is now false
                    if(!v) i->current = current;
                }
            }
            else {
                skipBraces(i);
            }
            return true;
        } else if(equalsString(id.value, "fun")) {
            optionalSlice name = consumeIdentifier(i);
            if(name.valid) {
                Function f;
                uint64_t n = 0;
                consumeOrFail(i, "(");
                char* start = i->current;
                //count number of params
                while(!consume(i, ")")) {
                    if(n != 0) consumeOrFail(i, ",");
                    consumeIdentifier(i);
                    n++;
                }
                char* end = i->current;

                i->current = start;
                Slice* parameters = (Slice*)malloc(sizeof(Slice)*n);
                //assign values to params
                for(size_t p = 0; p < n; p++) {
                    if(p != 0) consumeOrFail(i, ",");
                    optionalSlice s = consumeIdentifier(i);
                    if(s.valid) {
                        parameters[p] = s.value;
                    }
                    else {
                        fail(i);
                    }
                }
                i->current = end;
                //store values
                f.numParams = n;
                f.parameters = parameters;
                f.start = i->current;
                putF(i->functionTable, name.value, f);
                skipBraces(i);
            }
            else {
                fail(i);
            }
            return true;
        } else if (equalsString(id.value, "return")) {
            i->ret.value = expression(i, effects, inFunction);
            i->ret.valid = true;
            return true;
        } else if (consume(i, "=")) {
            uint64_t v = expression(i, effects, inFunction);
            if (effects) {
                //access according to where it is defined in the scope
                if(inFunction) {
                    if(get(*(i->localTable), id.value).valid) {
                        put(i->localTable, id.value, v);
                    } else if(get(*(i->symbolTable), id.value).valid) {
                        put(i->symbolTable, id.value, v);
                    }
                    else {
                        put(i->localTable, id.value, v);
                    }
                }
                else {
                    put(i->symbolTable, id.value, v);
                }
            }
            return true;
        } else if (consume(i, "(")) {
            runFunction(i, effects, true, id);
            return true;
        } else {
            fail(i);
        }
    }
    return false;
}

void statements(Interpreter* i, bool effects) {
    while (statement(i, effects, false));
}

void run(Interpreter* i) {
    statements(i, true);
    endOrFail(i);
};

void clearInterpreter(Interpreter i) {
    clearMap(*(i.symbolTable));
    clearMap(*(i.localTable));
    clearMapF(*(i.functionTable));
    free(i.symbolTable);
    free(i.localTable);
    free(i.functionTable);
}

uint64_t runFunction(Interpreter* i, bool effects, bool inFunction, optionalSlice id) {
    //create a temporary local scope
    Map* localMap = createMap();
    Function f = getF(*(i->functionTable), id.value);
    //assign values to parameters
    for(size_t p = 0; p < f.numParams; p++) {
        put(localMap, f.parameters[p], expression(i, effects, inFunction));
        if(p != f.numParams - 1) consumeOrFail(i, ",");
    }
    //store old scope and temporarily use new scope
    Map* currentTable = i->localTable;
    i->localTable = localMap;
    consumeOrFail(i, ")");
    char* current = i->current;
    i->current = f.start; 
    //run statement until end brace reached and no return value indicated
    consumeOrFail(i, "{");
    while(!consume(i, "}") && !(i->ret.valid)) {
        statement(i, effects, true);
    }
    //assign back to old values
    i->current = current;
    i->ret.valid = false;
    i->localTable = currentTable;
    uint64_t returnVal = i->ret.value;
    i->ret.value = 0;
    
    //clear memory not needed anymore
    clearMap(*localMap);
    free(localMap);
    return returnVal; 
}