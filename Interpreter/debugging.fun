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

x = f1(5)
y = f2(x, f1(5) * 2)
z = f3(x, y, f2(x, f1(5)*4))
print(z)