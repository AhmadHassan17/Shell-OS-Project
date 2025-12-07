#define _GNU_SOURCE
#include "shell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALIAS_MAX 100

typedef struct alias {
    char *name;
    char *value;
    struct alias *next;
} alias_t;

static alias_t *aliases = NULL;
static int alias_count = 0;

static char *alias_expand_recursive(const char *line, int depth);

void aliases_init(void) {
    // Could load from ~/.minishell_aliases if desired
}

void alias_set(const char *name, const char *value) {
    // Remove existing alias with same name
    alias_t *prev = NULL;
    alias_t *a = aliases;
    while (a) {
        if (strcmp(a->name, name) == 0) {
            if (prev)
                prev->next = a->next;
            else
                aliases = a->next;
            free(a->name);
            free(a->value);
            free(a);
            alias_count--;
            break;
        }
        prev = a;
        a = a->next;
    }
    
    if (alias_count >= ALIAS_MAX) {
        fprintf(stderr, "alias: too many aliases\n");
        return;
    }
    
    a = xmalloc(sizeof(*a));
    a->name = xstrdup(name);
    a->value = xstrdup(value);
    a->next = aliases;
    aliases = a;
    alias_count++;
}

void alias_unset(const char *name) {
    alias_t *prev = NULL;
    alias_t *a = aliases;
    while (a) {
        if (strcmp(a->name, name) == 0) {
            if (prev)
                prev->next = a->next;
            else
                aliases = a->next;
            free(a->name);
            free(a->value);
            free(a);
            alias_count--;
            return;
        }
        prev = a;
        a = a->next;
    }
}

const char *alias_get(const char *name) {
    alias_t *a = aliases;
    while (a) {
        if (strcmp(a->name, name) == 0)
            return a->value;
        a = a->next;
    }
    return NULL;
}

void alias_print(const char *name) {
    if (name) {
        const char *val = alias_get(name);
        if (val)
            printf("alias %s='%s'\n", name, val);
        else
            fprintf(stderr, "alias: %s: not found\n", name);
    } else {
        // Print all aliases
        alias_t *a = aliases;
        while (a) {
            printf("alias %s='%s'\n", a->name, a->value);
            a = a->next;
        }
    }
}

char *alias_expand(const char *line) {
    return alias_expand_recursive(line, 0);
}

static char *alias_expand_recursive(const char *line, int depth) {
    if (depth > 10) return NULL; // Prevent infinite recursion
    
    if (!line || !*line) return NULL;
    
    // Skip leading whitespace
    while (*line && (*line == ' ' || *line == '\t'))
        line++;
    
    // Find first word (command name)
    const char *end = line;
    while (*end && *end != ' ' && *end != '\t' && *end != '|' && 
           *end != ';' && *end != '&' && *end != '<' && *end != '>')
        end++;
    
    if (end == line) return NULL; // No command found
    
    size_t cmd_len = end - line;
    char *cmd_name = xmalloc(cmd_len + 1);
    memcpy(cmd_name, line, cmd_len);
    cmd_name[cmd_len] = '\0';
    
    const char *alias_val = alias_get(cmd_name);
    free(cmd_name);
    
    if (!alias_val) return NULL; // No alias found
    
    // Build expanded line: alias_value + rest of original line
    size_t rest_len = strlen(end);
    size_t val_len = strlen(alias_val);
    char *expanded = xmalloc(val_len + rest_len + 1);
    memcpy(expanded, alias_val, val_len);
    memcpy(expanded + val_len, end, rest_len);
    expanded[val_len + rest_len] = '\0';
    
    // Check if expanded value itself contains an alias (recursive expansion)
    char *further_expanded = alias_expand_recursive(expanded, depth + 1);
    if (further_expanded) {
        free(expanded);
        return further_expanded;
    }
    
    return expanded;
}

void aliases_cleanup(void) {
    alias_t *a = aliases;
    while (a) {
        alias_t *next = a->next;
        free(a->name);
        free(a->value);
        free(a);
        a = next;
    }
    aliases = NULL;
    alias_count = 0;
}

