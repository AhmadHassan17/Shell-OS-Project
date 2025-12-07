#define _GNU_SOURCE
#include "shell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HISTORY_MAX 1000

static char *history[HISTORY_MAX];
static int history_count = 0;
static int history_current = -1; // -1 means not browsing history

void history_init(void) {
    // Load from ~/.minishell_history if it exists
    const char *home = getenv("HOME");
    if (!home) return;
    
    char path[512];
    snprintf(path, sizeof(path), "%s/.minishell_history", home);
    
    FILE *f = fopen(path, "r");
    if (!f) return;
    
    char *line = NULL;
    size_t cap = 0;
    while (history_count < HISTORY_MAX && getline(&line, &cap, f) != -1) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[len - 1] = '\0';
        if (len > 1) { // Skip empty lines
            history[history_count++] = line;
            line = NULL;
            cap = 0;
        }
    }
    free(line);
    fclose(f);
}

void history_add(const char *line) {
    if (!line || !*line) return;
    
    // Don't add if it's the same as the last command
    if (history_count > 0 && strcmp(history[history_count - 1], line) == 0)
        return;
    
    if (history_count >= HISTORY_MAX) {
        free(history[0]);
        memmove(history, history + 1, (HISTORY_MAX - 1) * sizeof(char*));
        history_count = HISTORY_MAX - 1;
    }
    
    history[history_count++] = xstrdup(line);
    history_current = -1;
    
    // Save to file
    const char *home = getenv("HOME");
    if (!home) return;
    
    char path[512];
    snprintf(path, sizeof(path), "%s/.minishell_history", home);
    FILE *f = fopen(path, "a");
    if (f) {
        fprintf(f, "%s\n", line);
        fclose(f);
    }
}

const char *history_get(int direction) {
    if (history_count == 0) return NULL;
    
    if (history_current == -1) {
        // Start browsing from the end
        history_current = history_count;
    }
    
    if (direction > 0) {
        // Up arrow - go to older command
        if (history_current > 0)
            history_current--;
        else
            return NULL;
    } else {
        // Down arrow - go to newer command
        if (history_current < history_count - 1)
            history_current++;
        else {
            history_current = history_count; // Back to empty/new line
            return NULL;
        }
    }
    
    return history[history_current];
}

void history_reset_browse(void) {
    history_current = -1;
}

void history_print(void) {
    for (int i = 0; i < history_count; i++) {
        printf("%5d  %s\n", i + 1, history[i]);
    }
}

void history_cleanup(void) {
    for (int i = 0; i < history_count; i++) {
        free(history[i]);
        history[i] = NULL;
    }
    history_count = 0;
    history_current = -1;
}

