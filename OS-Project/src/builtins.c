#define _GNU_SOURCE
#include "shell.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int bi_cd(command_t *cmd) {
    const char *dir = NULL;
    if (!cmd->argv[1]) {
        dir = getenv("HOME");
        if (!dir)
            dir = "/";
    } else {
        dir = cmd->argv[1];
    }
    if (chdir(dir) < 0) {
        perror("cd");
        return 1;
    }
    return 0;
}

static int bi_pwd(void) {
    char buf[512];
    if (!getcwd(buf, sizeof(buf))) {
        perror("pwd");
        return 1;
    }
    puts(buf);
    return 0;
}

static int bi_exit(shell_state_t *sh, command_t *cmd) {
    int code = sh->last_status;
    if (cmd->argv[1])
        code = atoi(cmd->argv[1]);
    sh->running = false;
    return code;
}

static int bi_export(command_t *cmd) {
    for (int i = 1; cmd->argv[i]; i++) {
        char *eq = strchr(cmd->argv[i], '=');
        if (!eq) {
            fprintf(stderr, "export: invalid format: %s\n", cmd->argv[i]);
            continue;
        }
        *eq = '\0';
        const char *name = cmd->argv[i];
        const char *val = eq + 1;
        if (setenv(name, val, 1) < 0) {
            perror("setenv");
            *eq = '=';
            return 1;
        }
        *eq = '=';
    }
    return 0;
}

static int bi_unset(command_t *cmd) {
    for (int i = 1; cmd->argv[i]; i++) {
        if (unsetenv(cmd->argv[i]) < 0) {
            perror("unsetenv");
            return 1;
        }
    }
    return 0;
}

static int bi_echo(command_t *cmd) {
    int i = 1;
    int newline = 1;
    if (cmd->argv[1] && strcmp(cmd->argv[1], "-n") == 0) {
        newline = 0;
        i = 2;
    }
    bool first = true;
    for (; cmd->argv[i]; i++) {
        if (!first)
            fputc(' ', stdout);
        fputs(cmd->argv[i], stdout);
        first = false;
    }
    if (newline)
        fputc('\n', stdout);
    fflush(stdout);
    return 0;
}
static int bi_grep(command_t *cmd) {
    if (!cmd->argv[1]) {
        fprintf(stderr, "grep: missing PATTERN\n");
        return 1;
    }
    const char *pattern = cmd->argv[1];
    int exit_status = 1; // default: no matches

    char *line = NULL;
    size_t cap = 0;

    if (!cmd->argv[2]) {
        // Read from stdin
        while (getline(&line, &cap, stdin) != -1) {
            if (strstr(line, pattern)) {
                fputs(line, stdout);
                exit_status = 0;
            }
        }
    } else {
        for (int i = 2; cmd->argv[i]; i++) {
            const char *fname = cmd->argv[i];
            FILE *f = fopen(fname, "r");
            if (!f) {
                perror(fname);
                continue;
            }
            while (getline(&line, &cap, f) != -1) {
                if (strstr(line, pattern)) {
                    fputs(line, stdout);
                    exit_status = 0;
                }
            }
            fclose(f);
        }
    }

    free(line);
    return exit_status;
}

static int bi_ls(command_t *cmd) {
    int status = 0;
    
    // If no arguments, list current directory
    if (!cmd->argv[1]) {
        DIR *dir = opendir(".");
        if (!dir) {
            perror("ls");
            return 1;
        }
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
                continue;
            puts(ent->d_name);
        }
        closedir(dir);
        return 0;
    }
    
    // Handle multiple arguments (files/directories)
    for (int i = 1; cmd->argv[i]; i++) {
        const char *path = cmd->argv[i];
        struct stat st;
        
        if (stat(path, &st) < 0) {
            perror(path);
            status = 1;
            continue;
        }
        
        if (S_ISDIR(st.st_mode)) {
            // It's a directory - list its contents
            DIR *dir = opendir(path);
            if (!dir) {
                perror(path);
                status = 1;
                continue;
            }
            if (cmd->argv[2]) {
                // Multiple arguments, show directory name
                printf("%s:\n", path);
            }
            struct dirent *ent;
            while ((ent = readdir(dir)) != NULL) {
                if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
                    continue;
                puts(ent->d_name);
            }
            closedir(dir);
            if (cmd->argv[i + 1]) {
                printf("\n");
            }
        } else {
            // It's a file - just print the filename
            puts(path);
        }
    }
    return status;
}

static int bi_alias(command_t *cmd) {
    if (!cmd->argv[1]) {
        // Print all aliases
        alias_print(NULL);
        return 0;
    }
    
    // Check if it's "alias name=value" format
    for (int i = 1; cmd->argv[i]; i++) {
        char *eq = strchr(cmd->argv[i], '=');
        if (eq) {
            *eq = '\0';
            const char *name = cmd->argv[i];
            const char *value = eq + 1;
            alias_set(name, value);
            *eq = '=';
        } else {
            // Just print the alias
            alias_print(cmd->argv[i]);
        }
    }
    return 0;
}

static int bi_unalias(command_t *cmd) {
    if (!cmd->argv[1]) {
        fprintf(stderr, "unalias: missing argument\n");
        return 1;
    }
    for (int i = 1; cmd->argv[i]; i++) {
        alias_unset(cmd->argv[i]);
    }
    return 0;
}

static int bi_history(command_t *cmd) {
    (void)cmd;
    history_print();
    return 0;
}

static int bi_touch(command_t *cmd) {
    if (!cmd->argv[1]) {
        fprintf(stderr, "touch: missing file operand\n");
        return 1;
    }
    
    int status = 0;
    for (int i = 1; cmd->argv[i]; i++) {
        const char *filename = cmd->argv[i];
        int fd = open(filename, O_CREAT | O_WRONLY, 0666);
        if (fd < 0) {
            perror(filename);
            status = 1;
            continue;
        }
        // Update access and modification times
        if (utimensat(AT_FDCWD, filename, NULL, 0) < 0) {
            // If utimensat fails, try to update by writing
            if (write(fd, "", 0) < 0 && errno != EISDIR) {
                perror(filename);
                status = 1;
            }
        }
        close(fd);
    }
    return status;
}

static int bi_mkdir(command_t *cmd) {
    if (!cmd->argv[1]) {
        fprintf(stderr, "mkdir: missing operand\n");
        return 1;
    }
    
    bool parents = false;
    int start = 1;
    
    // Check for -p flag
    if (cmd->argv[1] && strcmp(cmd->argv[1], "-p") == 0) {
        parents = true;
        start = 2;
        if (!cmd->argv[2]) {
            fprintf(stderr, "mkdir: missing operand\n");
            return 1;
        }
    }
    
    int status = 0;
    for (int i = start; cmd->argv[i]; i++) {
        const char *dirname = cmd->argv[i];
        
        if (parents) {
            // Create parent directories as needed
            char *path = xstrdup(dirname);
            size_t len = strlen(path);
            
            // Create each directory component
            for (size_t i = 1; i <= len; i++) {
                if (path[i] == '/' || path[i] == '\0') {
                    char saved = path[i];
                    path[i] = '\0';
                    if (mkdir(path, 0755) < 0 && errno != EEXIST) {
                        perror(path);
                        status = 1;
                        free(path);
                        goto next_dir;
                    }
                    path[i] = saved;
                }
            }
            free(path);
        next_dir:
            (void)0; // Label for goto
        } else {
            if (mkdir(dirname, 0755) < 0) {
                perror(dirname);
                status = 1;
            }
        }
    }
    return status;
}

static int remove_recursive(const char *path) {
    struct stat st;
    if (lstat(path, &st) < 0) {
        return -1;
    }
    
    if (S_ISDIR(st.st_mode)) {
        DIR *dir = opendir(path);
        if (!dir) {
            return -1;
        }
        
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
                continue;
            
            char *subpath = xmalloc(strlen(path) + strlen(ent->d_name) + 2);
            snprintf(subpath, strlen(path) + strlen(ent->d_name) + 2, "%s/%s", path, ent->d_name);
            
            if (remove_recursive(subpath) < 0) {
                free(subpath);
                closedir(dir);
                return -1;
            }
            free(subpath);
        }
        closedir(dir);
        return rmdir(path);
    } else {
        return unlink(path);
    }
}

static int bi_rm(command_t *cmd) {
    if (!cmd->argv[1]) {
        fprintf(stderr, "rm: missing operand\n");
        return 1;
    }
    
    bool recursive = false;
    bool force = false;
    int start = 1;
    
    // Parse flags
    for (int i = 1; cmd->argv[i]; i++) {
        if (cmd->argv[i][0] == '-') {
            for (int j = 1; cmd->argv[i][j]; j++) {
                if (cmd->argv[i][j] == 'r' || cmd->argv[i][j] == 'R') {
                    recursive = true;
                } else if (cmd->argv[i][j] == 'f') {
                    force = true;
                } else if (cmd->argv[i][j] != '-') {
                    // Unknown flag, but continue
                }
            }
            start = i + 1;
        } else {
            break;
        }
    }
    
    if (!cmd->argv[start]) {
        fprintf(stderr, "rm: missing operand\n");
        return 1;
    }
    
    int status = 0;
    for (int i = start; cmd->argv[i]; i++) {
        const char *path = cmd->argv[i];
        
        struct stat st;
        if (lstat(path, &st) < 0) {
            if (!force) {
                perror(path);
                status = 1;
            }
            continue;
        }
        
        if (S_ISDIR(st.st_mode)) {
            if (!recursive) {
                if (!force) {
                    fprintf(stderr, "rm: cannot remove '%s': Is a directory\n", path);
                }
                status = 1;
                continue;
            }
            if (remove_recursive(path) < 0) {
                if (!force) {
                    perror(path);
                }
                status = 1;
            }
        } else {
            if (unlink(path) < 0) {
                if (!force) {
                    perror(path);
                }
                status = 1;
            }
        }
    }
    return status;
}

static int bi_cat(command_t *cmd) {
    if (!cmd->argv[1]) {
        // No arguments - read from stdin
        char *line = NULL;
        size_t cap = 0;
        bool printed_anything = false;
        while (getline(&line, &cap, stdin) != -1) {
            fputs(line, stdout);
            printed_anything = true;
        }
        free(line);
        // Ensure newline at end
        if (printed_anything) {
            fputc('\n', stdout);
        }
        fflush(stdout);
        return 0;
    }
    
    int status = 0;
    bool printed_anything = false;
    bool last_was_newline = true;
    
    for (int i = 1; cmd->argv[i]; i++) {
        const char *filename = cmd->argv[i];
        FILE *f = fopen(filename, "r");
        if (!f) {
            perror(filename);
            status = 1;
            continue;
        }
        
        char *line = NULL;
        size_t cap = 0;
        while (getline(&line, &cap, f) != -1) {
            fputs(line, stdout);
            printed_anything = true;
            // Check if line ends with newline (getline includes it)
            size_t len = strlen(line);
            last_was_newline = (len > 0 && line[len - 1] == '\n');
        }
        free(line);
        fclose(f);
    }
    
    // Ensure newline at end if we printed anything and last line didn't end with newline
    if (printed_anything && !last_was_newline) {
        fputc('\n', stdout);
    }
    fflush(stdout);
    
    return status;
}

bool is_builtin(const char *name) {
    if (!name) return false;
    return strcmp(name, "cd") == 0 ||
           strcmp(name, "pwd") == 0 ||
           strcmp(name, "exit") == 0 ||
           strcmp(name, "export") == 0 ||
           strcmp(name, "unset") == 0 ||
           strcmp(name, "jobs") == 0 ||
           strcmp(name, "echo") == 0 ||
           strcmp(name, "grep") == 0 ||
           strcmp(name, "ls") == 0 ||
           strcmp(name, "alias") == 0 ||
           strcmp(name, "unalias") == 0 ||
           strcmp(name, "history") == 0 ||
           strcmp(name, "touch") == 0 ||
           strcmp(name, "mkdir") == 0 ||
           strcmp(name, "rm") == 0 ||
           strcmp(name, "cat") == 0;
}

int run_builtin(shell_state_t *sh, command_t *cmd) {
    const char *name = cmd->argv[0];
    if (strcmp(name, "cd") == 0)
        return bi_cd(cmd);
    if (strcmp(name, "pwd") == 0)
        return bi_pwd();
    if (strcmp(name, "exit") == 0)
        return bi_exit(sh, cmd);
    if (strcmp(name, "export") == 0)
        return bi_export(cmd);
    if (strcmp(name, "unset") == 0)
        return bi_unset(cmd);
    if (strcmp(name, "jobs") == 0) {
        jobs_print();
        return 0;
    }
    if (strcmp(name, "echo") == 0)
        return bi_echo(cmd);
    if (strcmp(name, "grep") == 0)
        return bi_grep(cmd);
    if (strcmp(name, "ls") == 0)
        return bi_ls(cmd);
    if (strcmp(name, "alias") == 0)
        return bi_alias(cmd);
    if (strcmp(name, "unalias") == 0)
        return bi_unalias(cmd);
    if (strcmp(name, "history") == 0)
        return bi_history(cmd);
    if (strcmp(name, "touch") == 0)
        return bi_touch(cmd);
    if (strcmp(name, "mkdir") == 0)
        return bi_mkdir(cmd);
    if (strcmp(name, "rm") == 0)
        return bi_rm(cmd);
    if (strcmp(name, "cat") == 0)
        return bi_cat(cmd);
    return 1;
}


