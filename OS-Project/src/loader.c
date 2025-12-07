#define _GNU_SOURCE
#include "shell.h"

#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

extern void loader_trampoline(void *entry, void *stack_top);

static void die(const char *msg) {
    perror(msg);
    _exit(127);
}

static void *map_stack(size_t size, void **out_top) {
    void *base = mmap(NULL, size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (base == MAP_FAILED)
        die("mmap stack");
    *out_top = (char *)base + size;
    return base;
}

static void build_initial_stack(void *stack_base, void **stack_top,
                                const char *path,
                                char *const argv[],
                                char *const envp[]) {
    (void)stack_base;
    uintptr_t sp = (uintptr_t)*stack_top;

    int argc = 0;
    while (argv[argc])
        argc++;
    int envc = 0;
    while (envp && envp[envc])
        envc++;

    size_t total_len = strlen(path) + 1;
    for (int i = 0; i < argc; i++)
        total_len += strlen(argv[i]) + 1;
    for (int i = 0; i < envc; i++)
        total_len += strlen(envp[i]) + 1;

    sp &= ~0xFul;

    sp -= total_len;
    char *str_base = (char *)sp;
    char *p = str_base;

    char *path_copy = p;
    memcpy(p, path, strlen(path) + 1);
    p += strlen(path) + 1;

    char **argv_ptrs = alloca((argc + 1) * sizeof(char *));
    char **envp_ptrs = alloca((envc + 1) * sizeof(char *));

    for (int i = 0; i < argc; i++) {
        argv_ptrs[i] = p;
        size_t len = strlen(argv[i]) + 1;
        memcpy(p, argv[i], len);
        p += len;
    }
    argv_ptrs[argc] = NULL;

    for (int i = 0; i < envc; i++) {
        envp_ptrs[i] = p;
        size_t len = strlen(envp[i]) + 1;
        memcpy(p, envp[i], len);
        p += len;
    }
    envp_ptrs[envc] = NULL;

    sp &= ~0xFul;

    sp -= sizeof(uintptr_t) * 2;
    uintptr_t *auxv = (uintptr_t *)sp;
    auxv[0] = 0;
    auxv[1] = 0;

    sp -= sizeof(uintptr_t) * (envc + 1);
    uintptr_t *envp_area = (uintptr_t *)sp;
    for (int i = 0; i < envc; i++)
        envp_area[i] = (uintptr_t)envp_ptrs[i];
    envp_area[envc] = 0;

    sp -= sizeof(uintptr_t) * (argc + 1);
    uintptr_t *argv_area = (uintptr_t *)sp;
    for (int i = 0; i < argc; i++)
        argv_area[i] = (uintptr_t)argv_ptrs[i];
    argv_area[argc] = 0;

    sp -= sizeof(uintptr_t);
    uintptr_t *argc_area = (uintptr_t *)sp;
    *argc_area = (uintptr_t)argc;

    (void)path_copy;
    *stack_top = (void *)sp;
}

int loader_run_elf(const char *path, char *const argv[], char *const envp[]) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror(path);
        return 127;
    }

    Elf64_Ehdr eh;
    if (read(fd, &eh, sizeof(eh)) != sizeof(eh)) {
        perror("read ELF header");
        close(fd);
        return 127;
    }

    if (memcmp(eh.e_ident, ELFMAG, SELFMAG) != 0 ||
        eh.e_ident[EI_CLASS] != ELFCLASS64 ||
        eh.e_type != ET_EXEC ||
        eh.e_machine != EM_X86_64) {
        fprintf(stderr, "%s: unsupported ELF file\n", path);
        close(fd);
        return 127;
    }

    if (eh.e_phentsize != sizeof(Elf64_Phdr) || eh.e_phnum <= 0) {
        fprintf(stderr, "%s: bad program headers\n", path);
        close(fd);
        return 127;
    }

    Elf64_Phdr *phdrs = malloc(eh.e_phnum * sizeof(Elf64_Phdr));
    if (!phdrs)
        die("malloc phdrs");

    if (lseek(fd, eh.e_phoff, SEEK_SET) < 0)
        die("lseek phdrs");
    if (read(fd, phdrs, eh.e_phnum * sizeof(Elf64_Phdr)) !=
        (ssize_t)(eh.e_phnum * sizeof(Elf64_Phdr)))
        die("read phdrs");

    for (int i = 0; i < eh.e_phnum; i++) {
        if (phdrs[i].p_type == PT_INTERP) {
            fprintf(stderr, "%s: dynamic executables not supported, use -static\n", path);
            free(phdrs);
            close(fd);
            return 127;
        }
    }

    for (int i = 0; i < eh.e_phnum; i++) {
        Elf64_Phdr *ph = &phdrs[i];
        if (ph->p_type != PT_LOAD)
            continue;

        Elf64_Off off = ph->p_offset;
        Elf64_Addr vaddr = ph->p_vaddr;
        size_t filesz = ph->p_filesz;
        size_t memsz = ph->p_memsz;

        Elf64_Addr page = vaddr & ~(Elf64_Addr)(0xFFF);
        Elf64_Addr page_off = vaddr - page;
        size_t map_sz = page_off + memsz;

        int prot = 0;
        if (ph->p_flags & PF_R) prot |= PROT_READ;
        if (ph->p_flags & PF_W) prot |= PROT_WRITE;
        if (ph->p_flags & PF_X) prot |= PROT_EXEC;

        void *addr = mmap((void *)page, map_sz, prot,
                          MAP_PRIVATE | MAP_FIXED, fd, off - page_off);
        if (addr == MAP_FAILED)
            die("mmap segment");

        if (memsz > filesz) {
            memset((char *)vaddr + filesz, 0, memsz - filesz);
        }
    }

    free(phdrs);
    close(fd);

    size_t stack_size = 8 * 1024 * 1024;
    void *stack_top;
    void *stack_base = map_stack(stack_size, &stack_top);
    (void)stack_base;

    build_initial_stack(stack_base, &stack_top, path, argv, envp);

    loader_trampoline((void *)(uintptr_t)eh.e_entry, stack_top);
    __builtin_unreachable();
}




