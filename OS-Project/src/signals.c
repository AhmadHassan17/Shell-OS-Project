#define _GNU_SOURCE
#include "shell.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void sigint_handler(int signo) {
    (void)signo;
    write(STDOUT_FILENO, "\n", 1);
}

void signals_init(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa, NULL) < 0) {
        perror("sigaction");
    }
    signal(SIGTSTP, SIG_IGN);
}


