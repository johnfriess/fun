    .data
format: .byte '%', 'l', 'u', 10, 0
    .text
    .global main
print:
    push %rbp
    mov %rsp, %rbp
    mov $0, %rax
    lea format(%rip), %rdi
    mov 16(%rsp), %rsi
    .extern printf
    and $0xFFFFFFFFFFFFFFF0, %rsp
    call printf
    mov $0, %rax
    mov %rbp, %rsp
    pop %rbp
    ret
main:
    call ._main
    ret
._stuff:
    push %rbp
    mov %rsp, %rbp
._stuff._start:
    sub $0, %rsp
    push 16(%rbp)
    call print
    pop %rsi
    push 16(%rbp)
    pop %rax
    mov %rbp, %rsp
    pop %rbp
    ret
    mov %rbp, %rsp
    pop %rbp
    mov $0, %rax
    ret
._main:
    push %rbp
    mov %rsp, %rbp
._main._start:
    sub $0, %rsp
    mov $20, %rdi
    push %rdi
    call ._stuff
    add $8, %rsp
    mov %rbp, %rsp
    pop %rbp
    mov $0, %rax
    ret
