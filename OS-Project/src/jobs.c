#include "shell.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct job {
    pid_t pgid;
    char *cmdline;
    bool  background;
    struct job *next;
} job_t;

static job_t *jobs_head = NULL;

void jobs_init(void) {
    jobs_head = NULL;
}

void jobs_add(pid_t pgid, const char *cmdline, bool background) {
    job_t *j = xmalloc(sizeof(*j));
    j->pgid = pgid;
    j->cmdline = xstrdup(cmdline);
    j->background = background;
    j->next = jobs_head;
    jobs_head = j;
}

static void jobs_remove(pid_t pgid) {
    job_t **pp = &jobs_head;
    while (*pp) {
        if ((*pp)->pgid == pgid) {
            job_t *dead = *pp;
            *pp = dead->next;
            free(dead->cmdline);
            free(dead);
            return;
        }
        pp = &(*pp)->next;
    }
}

void jobs_reap(bool blocking) {
    int status;
    int options = blocking ? 0 : WNOHANG;
    pid_t pid;

    while ((pid = waitpid(-1, &status, options)) > 0) {
        printf("[bg] process %d finished\n", pid);
        jobs_remove(pid);
    }
    if (pid < 0 && errno != ECHILD && errno != EINTR) {
        perror("waitpid");
    }
}

void jobs_print(void) {
    job_t *j = jobs_head;
    while (j) {
        printf("[%d] %s %s\n", (int)j->pgid,
               j->background ? "Running" : "Done",
               j->cmdline);
        j = j->next;
    }
}




