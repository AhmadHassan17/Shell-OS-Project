#define _GNU_SOURCE
#include "shell.h"

#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void repl(shell_state_t *sh) {
    char *line = NULL;
    size_t cap = 0;

    while (sh->running) {
        jobs_reap(false);

        char *prompt = get_prompt();
        fputs(prompt, stdout);
        fflush(stdout);

        ssize_t n = read_line_with_history(&line, &cap);
        if (n < 0) {
            if (feof(stdin)) {
                fputc('\n', stdout);
                break;
            }
            if (errno == EINTR) {
                clearerr(stdin);
                continue;
            }
            perror("read_line_with_history");
            break;
        }
        if (n > 0 && line[n - 1] == '\n')
            line[n - 1] = '\0';
        
        if (!line || !*line) {
            continue;
        }

        // Expand aliases
        char *expanded = alias_expand(line);
        const char *cmd_line = expanded ? expanded : line;

        // Add to history (before alias expansion for user visibility)
        history_add(line);

        command_t *cmd = parse_line(cmd_line);
        if (expanded) free(expanded);
        if (!cmd)
            continue;
        sh->last_status = execute_commands(sh, cmd);
        free_command_list(cmd);
    }
    free(line);
    input_cleanup();
}

int main(int argc, char **argv) {
    shell_state_t sh;
    sh.last_status = 0;
    sh.running = true;

    signals_init();
    jobs_init();
    history_init();
    aliases_init();

    // If -c flag is provided, execute command and exit
    if (argc > 2 && strcmp(argv[1], "-c") == 0) {
        // Expand aliases for -c mode too
        char *expanded = alias_expand(argv[2]);
        const char *cmd_line = expanded ? expanded : argv[2];
        command_t *cmd = parse_line(cmd_line);
        if (expanded) free(expanded);
        if (cmd) {
            sh.last_status = execute_commands(&sh, cmd);
            free_command_list(cmd);
        }
        aliases_cleanup();
        history_cleanup();
        return sh.last_status;
    }

    // Otherwise, run in interactive mode (continuous loop)
    repl(&sh);
    aliases_cleanup();
    history_cleanup();
    return sh.last_status;
}


