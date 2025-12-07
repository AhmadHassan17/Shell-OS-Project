#define _GNU_SOURCE
#include "shell.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_COMPLETIONS 1000

typedef struct {
    char **items;
    int count;
} completion_list_t;

static void completion_list_init(completion_list_t *list) {
    list->items = NULL;
    list->count = 0;
}

static void completion_list_add(completion_list_t *list, const char *item) {
    if (list->count >= MAX_COMPLETIONS) return;
    
    if (!list->items) {
        list->items = xmalloc(MAX_COMPLETIONS * sizeof(char*));
    }
    
    list->items[list->count++] = xstrdup(item);
}

static void completion_list_free(completion_list_t *list) {
    if (list->items) {
        for (int i = 0; i < list->count; i++) {
            free(list->items[i]);
        }
        free(list->items);
        list->items = NULL;
    }
    list->count = 0;
}

static int completion_compare(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

// Get builtin commands
static void get_builtin_completions(const char *prefix, completion_list_t *list) {
    const char *builtins[] = {
        "cd", "pwd", "exit", "export", "unset", "jobs", "echo", "grep", "ls",
        "alias", "unalias", "history", "touch", "mkdir", "rm", "cat", NULL
    };
    
    size_t prefix_len = strlen(prefix);
    for (int i = 0; builtins[i]; i++) {
        if (strncmp(builtins[i], prefix, prefix_len) == 0) {
            completion_list_add(list, builtins[i]);
        }
    }
}

// Get executables from PATH
static void get_path_completions(const char *prefix, completion_list_t *list) {
    const char *path = getenv("PATH");
    if (!path) path = "/bin:/usr/bin";
    
    size_t prefix_len = strlen(prefix);
    char *path_copy = xstrdup(path);
    char *save = NULL;
    
    for (char *dir = strtok_r(path_copy, ":", &save); dir; dir = strtok_r(NULL, ":", &save)) {
        DIR *d = opendir(dir);
        if (!d) continue;
        
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            if (strncmp(ent->d_name, prefix, prefix_len) == 0) {
                // Check if it's executable (simplified - just check if it's a regular file)
                char full_path[512];
                snprintf(full_path, sizeof(full_path), "%s/%s", dir, ent->d_name);
                if (access(full_path, X_OK) == 0) {
                    // Check if already added
                    bool found = false;
                    for (int i = 0; i < list->count; i++) {
                        if (strcmp(list->items[i], ent->d_name) == 0) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        completion_list_add(list, ent->d_name);
                    }
                }
            }
        }
        closedir(d);
    }
    free(path_copy);
}

// Get filename completions from current directory
static void get_filename_completions(const char *prefix, completion_list_t *list) {
    char *cwd = getcwd(NULL, 0);
    if (!cwd) return;
    
    DIR *d = opendir(cwd);
    if (!d) {
        free(cwd);
        return;
    }
    
    size_t prefix_len = strlen(prefix);
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strncmp(ent->d_name, prefix, prefix_len) == 0) {
            // Skip . and ..
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
                continue;
            completion_list_add(list, ent->d_name);
        }
    }
    closedir(d);
    free(cwd);
}

// Determine if we're completing a command or filename
static bool is_filename_context(const char *line, size_t cursor) {
    // Look backwards from cursor to find the start of current word
    size_t start = cursor;
    while (start > 0 && line[start - 1] != ' ' && line[start - 1] != '\t' &&
           line[start - 1] != '|' && line[start - 1] != ';' && line[start - 1] != '&' &&
           line[start - 1] != '<' && line[start - 1] != '>') {
        start--;
    }
    
    // Check if there's a / in the current word (indicates path)
    for (size_t i = start; i < cursor; i++) {
        if (line[i] == '/') return true;
    }
    
    // Check if we're after a redirection operator
    if (start > 0) {
        if (line[start - 1] == '<' || line[start - 1] == '>') return true;
    }
    
    // Check if we're after the first word (command) - then it's likely a filename
    size_t first_word_end = 0;
    while (first_word_end < cursor && line[first_word_end] != ' ' && 
           line[first_word_end] != '\t' && line[first_word_end] != '\0') {
        first_word_end++;
    }
    
    if (start > first_word_end) return true;
    
    return false;
}

int complete_input(const char *line, size_t cursor, char **completion, int *list_pos) {
    // Find current word being completed
    size_t word_start = cursor;
    while (word_start > 0 && line[word_start - 1] != ' ' && line[word_start - 1] != '\t' &&
           line[word_start - 1] != '|' && line[word_start - 1] != ';' && line[word_start - 1] != '&' &&
           line[word_start - 1] != '<' && line[word_start - 1] != '>') {
        word_start--;
    }
    
    size_t word_len = cursor - word_start;
    char *prefix = xmalloc(word_len + 1);
    memcpy(prefix, line + word_start, word_len);
    prefix[word_len] = '\0';
    
    completion_list_t list;
    completion_list_init(&list);
    
    bool is_file = is_filename_context(line, cursor);
    
    if (is_file) {
        get_filename_completions(prefix, &list);
    } else {
        // Command completion
        get_builtin_completions(prefix, &list);
        get_path_completions(prefix, &list);
    }
    
    free(prefix);
    
    if (list.count == 0) {
        completion_list_free(&list);
        return 0; // No completions
    }
    
    // Sort completions
    qsort(list.items, list.count, sizeof(char*), completion_compare);
    
    if (list.count == 1) {
        // Single match - complete it
        *completion = xstrdup(list.items[0]);
        completion_list_free(&list);
        return 1;
    }
    
    // Multiple matches
    if (*list_pos < 0) {
        // First tab - show list with serial numbers
        // Print newline and ensure we're at start of line
        fputc('\r', stdout);  // Carriage return
        fputc('\n', stdout);  // Newline
        fputs("Available completions:\r\n", stdout);
        fputs("─────────────────────────────────────────────────────────────\r\n", stdout);
        for (int i = 0; i < list.count; i++) {
            fprintf(stdout, "  %2d. %s", i + 1, list.items[i]);
            // Add description for builtin commands
            if (strcmp(list.items[i], "cd") == 0) {
                fputs(" - Change directory", stdout);
            } else if (strcmp(list.items[i], "pwd") == 0) {
                fputs(" - Print working directory", stdout);
            } else if (strcmp(list.items[i], "exit") == 0) {
                fputs(" - Exit shell", stdout);
            } else if (strcmp(list.items[i], "export") == 0) {
                fputs(" - Set environment variable", stdout);
            } else if (strcmp(list.items[i], "unset") == 0) {
                fputs(" - Unset environment variable", stdout);
            } else if (strcmp(list.items[i], "jobs") == 0) {
                fputs(" - List background jobs", stdout);
            } else if (strcmp(list.items[i], "echo") == 0) {
                fputs(" - Print text", stdout);
            } else if (strcmp(list.items[i], "grep") == 0) {
                fputs(" - Search for pattern", stdout);
            } else if (strcmp(list.items[i], "ls") == 0) {
                fputs(" - List directory contents", stdout);
            } else if (strcmp(list.items[i], "alias") == 0) {
                fputs(" - Create/display aliases", stdout);
            } else if (strcmp(list.items[i], "unalias") == 0) {
                fputs(" - Remove alias", stdout);
            } else if (strcmp(list.items[i], "history") == 0) {
                fputs(" - Show command history", stdout);
            } else if (strcmp(list.items[i], "touch") == 0) {
                fputs(" - Create/update file timestamps", stdout);
            } else if (strcmp(list.items[i], "mkdir") == 0) {
                fputs(" - Create directory", stdout);
            } else if (strcmp(list.items[i], "rm") == 0) {
                fputs(" - Remove files/directories", stdout);
            } else if (strcmp(list.items[i], "cat") == 0) {
                fputs(" - Display file contents", stdout);
            }
            fputc('\r', stdout);
            fputc('\n', stdout);
        }
        fputs("─────────────────────────────────────────────────────────────\r\n", stdout);
        fputs("Press Tab again to cycle through matches, or type to continue.\r\n", stdout);
        fflush(stdout);
        *list_pos = 0;
    } else {
        // Cycle through matches
        *list_pos = (*list_pos + 1) % list.count;
    }
    
    *completion = xstrdup(list.items[*list_pos]);
    completion_list_free(&list);
    return list.count;
}

