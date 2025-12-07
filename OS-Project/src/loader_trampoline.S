.globl loader_trampoline
.type loader_trampoline,@function

// void loader_trampoline(void *entry, void *stack_top);
// rdi = entry, rsi = stack_top
loader_trampoline:
    mov %rsi, %rsp
    jmp *%rdi




