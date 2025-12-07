#define _GNU_SOURCE
#include "shell.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int loader_run_elf(const char *path, char *const argv[], char *const envp[]);

static char *find_in_path(const char *name) {
    if (strchr(name, '/'))
        return xstrdup(name);
    const char *path = getenv("PATH");
    if (!path)
        path = "/bin:/usr/bin";
    char *p = xstrdup(path);
    char *save = NULL;
    for (char *dir = strtok_r(p, ":", &save); dir; dir = strtok_r(NULL, ":", &save)) {
        size_t len = strlen(dir) + 1 + strlen(name) + 1;
        char *full = xmalloc(len);
        snprintf(full, len, "%s/%s", dir, name);
        if (access(full, X_OK) == 0) {
            free(p);
            return full;
        }
        free(full);
    }
    free(p);
    return NULL;
}

static int setup_redirs(redir_t *r) {
    for (; r; r = r->next) {
        int fd;
        if (r->type == REDIR_IN) {
            fd = open(r->filename, O_RDONLY);
            if (fd < 0) {
                perror(r->filename);
                return -1;
            }
            if (dup2(fd, STDIN_FILENO) < 0) {
                perror("dup2");
                close(fd);
                return -1;
            }
            close(fd);
        } else if (r->type == REDIR_OUT) {
            fd = open(r->filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (fd < 0) {
                perror(r->filename);
                return -1;
            }
            if (dup2(fd, STDOUT_FILENO) < 0) {
                perror("dup2");
                close(fd);
                return -1;
            }
            close(fd);
        } else if (r->type == REDIR_APPEND) {
            fd = open(r->filename, O_WRONLY | O_CREAT | O_APPEND, 0666);
            if (fd < 0) {
                perror(r->filename);
                return -1;
            }
            if (dup2(fd, STDOUT_FILENO) < 0) {
                perror("dup2");
                close(fd);
                return -1;
            }
            close(fd);
        }
    }
    return 0;
}

static int launch_process(command_t *cmd, int in_fd, int out_fd, pid_t pgid, int is_first, int is_background) {
    // Expand glob patterns for this command
    char **expanded_argv = expand_glob_patterns(cmd->argv);
    bool was_expanded = (expanded_argv != cmd->argv);
    
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        if (was_expanded) {
            for (int i = 0; expanded_argv[i]; i++) free(expanded_argv[i]);
            free(expanded_argv);
        }
        return -1;
    }
    if (pid == 0) {
        if (in_fd != STDIN_FILENO) {
            dup2(in_fd, STDIN_FILENO);
            close(in_fd);
        }
        if (out_fd != STDOUT_FILENO) {
            dup2(out_fd, STDOUT_FILENO);
            close(out_fd);
        }
        if (setup_redirs(cmd->redirs) < 0)
            _exit(127);

        setpgid(0, pgid ? pgid : 0);
        if (!is_background)
            tcsetpgrp(STDIN_FILENO, getpid());

        if (is_builtin(expanded_argv[0])) {
            command_t temp_cmd = *cmd;
            temp_cmd.argv = expanded_argv;
            shell_state_t dummy = {.last_status = 0, .running = true};
            int st = run_builtin(&dummy, &temp_cmd);
            if (was_expanded) {
                for (int i = 0; expanded_argv[i]; i++) free(expanded_argv[i]);
                free(expanded_argv);
            }
            exit(st); // use exit() so stdio buffers are flushed for pipelines
        }

        char *path = find_in_path(expanded_argv[0]);
        if (!path) {
            fprintf(stderr, "%s: command not found\n", expanded_argv[0]);
            if (was_expanded) {
                for (int i = 0; expanded_argv[i]; i++) free(expanded_argv[i]);
                free(expanded_argv);
            }
            _exit(127);
        }
        extern char **environ;
        loader_run_elf(path, expanded_argv, environ);
        if (was_expanded) {
            for (int i = 0; expanded_argv[i]; i++) free(expanded_argv[i]);
            free(expanded_argv);
        }
        _exit(127);
    } else {
        if (was_expanded) {
            for (int i = 0; expanded_argv[i]; i++) free(expanded_argv[i]);
            free(expanded_argv);
        }
        if (is_first)
            setpgid(pid, pid);
        else
            setpgid(pid, pgid);
        return pid;
    }
}

static int execute_pipeline(shell_state_t *sh, command_t *cmd) {
    (void)sh;
    int in_fd = STDIN_FILENO;
    int status = 0;
    int pipefd[2];
    pid_t pgid = 0;
    int num_procs = 0;

    for (command_t *c = cmd; c; c = c->next_pipe) {
        int out_fd = STDOUT_FILENO;
        if (c->next_pipe) {
            if (pipe(pipefd) < 0) {
                perror("pipe");
                return 1;
            }
            out_fd = pipefd[1];
        }

        int is_first = (c == cmd);
        int is_background = c->background;

        pid_t pid = launch_process(c, in_fd, out_fd, pgid, is_first, is_background);
        if (pid < 0) {
            status = 1;
        } else {
            num_procs++;
            if (is_first)
                pgid = pid;
        }

        if (in_fd != STDIN_FILENO)
            close(in_fd);
        if (c->next_pipe) {
            close(out_fd);
            in_fd = pipefd[0];
        }
    }

    if (!cmd->background && pgid > 0) {
        int wstatus;
        tcsetpgrp(STDIN_FILENO, pgid);
        pid_t w;
        do {
            w = waitpid(-pgid, &wstatus, 0);
        } while (w > 0 || (w < 0 && errno == EINTR));
        tcsetpgrp(STDIN_FILENO, getpgrp());
        if (WIFEXITED(wstatus))
            status = WEXITSTATUS(wstatus);
        else if (WIFSIGNALED(wstatus))
            status = 128 + WTERMSIG(wstatus);
    } else if (cmd->background && pgid > 0) {
        jobs_add(pgid, cmd->argv[0], true);
        printf("[bg] started %d\n", pgid);
    }

    return status;
}

int execute_commands(shell_state_t *sh, command_t *cmd) {
    int status = 0;
    for (command_t *c = cmd; c; c = c->next_seq) {
        // Expand glob patterns in argv
        char **expanded_argv = expand_glob_patterns(c->argv);
        bool was_expanded = (expanded_argv != c->argv);
        
        // Create temporary command with expanded argv
        command_t temp_cmd = *c;
        temp_cmd.argv = expanded_argv;
        
        if (!c->next_pipe && is_builtin(c->argv[0]) && !c->background) {
            if (setup_redirs(c->redirs) < 0)
                status = 1;
            else
                status = run_builtin(sh, &temp_cmd);
            sh->last_status = status;
        } else {
            status = execute_pipeline(sh, &temp_cmd);
            sh->last_status = status;
        }
        
        // Free expanded argv if it was expanded
        if (was_expanded) {
            for (int i = 0; expanded_argv[i]; i++) {
                free(expanded_argv[i]);
            }
            free(expanded_argv);
        }
    }
    return status;
}




