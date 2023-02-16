#this is a test to verify function compilation, even when they aren't called and defined after main

fun f1(x) {
    print(x)
    return x
}

fun f2(x, y) {
    print(x)
    print(y)
    return x + y
}

fun f3(x, y, z) {
    print(x)
    print(y)
    print(z)
    return x + y + z
}

fun f4(x, y, z, a) {
    print(x)
    print(y)
    print(z)
    print(a)
    return x + y + z + a
}

fun main() {
    x = f1(5)
    y = f2(x, f1(5) * 2)
    z = f3(x, y, f2(x, f1(5)*4))
    print(z)
}

#these functions should be compiled

fun f5(x, y, z, a, b) {
    print(x)
    print(y)
    print(z)
    print(a)
    print(b)
    return x + y + z + a + b
}

fun f6(x, y, z, a, b, c) {
    print(x)
    print(y)
    print(z)
    print(a)
    print(b)
    print(c)
    return x + y + z + a + b + c
}
