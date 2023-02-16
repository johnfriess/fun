fun functionCalling(this, test, makes, sure, that, the, call, stack, works, as, intended, while, also, performing, some, level, of, a, stress, test) {}

x = 0

fun comment1(function, assignment, calls, function, properly, and, returns, right, value) {}

fun f1(x) {
    print(x)
}

x = f1(x)
print(x)


fun comment2(recursive, call, test) {}

fun f2(x) {
    x = x + 1
    print(x)
    while(x < 1000) {
        return f2(x)
    }
}

f2(x)

fun comment3(function, calling, between, different, functions) {}

fun f3(x) {
    f4(x+1)
    print(x)
}

fun f4(x) {
    if(x < 1000) {
        f3(x)
    }
    print(x)
}

f3(x)