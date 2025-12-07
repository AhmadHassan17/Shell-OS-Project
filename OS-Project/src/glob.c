#define _GNU_SOURCE
#include "shell.h"

#include <dirent.h>
#include <fnmatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char **items;
    int count;
    int capacity;
} glob_list_t;

static void glob_list_init(glob_list_t *list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

static void glob_list_add(glob_list_t *list, const char *item) {
    if (list->count >= list->capacity) {
        list->capacity = list->capacity ? list->capacity * 2 : 16;
        list->items = realloc(list->items, list->capacity * sizeof(char*));
        if (!list->items) {
            perror("realloc");
            exit(1);
        }
    }
    list->items[list->count++] = xstrdup(item);
}

static void glob_list_free(glob_list_t *list) {
    if (list->items) {
        for (int i = 0; i < list->count; i++) {
            free(list->items[i]);
        }
        free(list->items);
    }
    glob_list_init(list);
}

static bool has_glob_chars(const char *pattern) {
    return strchr(pattern, '*') != NULL || strchr(pattern, '?') != NULL ||
           strchr(pattern, '[') != NULL;
}

static void expand_pattern_in_dir(const char *pattern, const char *dir, glob_list_t *list) {
    DIR *d = opendir(dir);
    if (!d) return;
    
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        // Skip . and ..
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;
        
        // Check if name matches pattern
        if (fnmatch(pattern, ent->d_name, 0) == 0) {
            // Build full path
            char *fullpath;
            if (strcmp(dir, ".") == 0) {
                fullpath = xstrdup(ent->d_name);
            } else {
                size_t len = strlen(dir) + 1 + strlen(ent->d_name) + 1;
                fullpath = xmalloc(len);
                snprintf(fullpath, len, "%s/%s", dir, ent->d_name);
            }
            glob_list_add(list, fullpath);
            free(fullpath);
        }
    }
    closedir(d);
}

static void expand_pattern(const char *pattern, glob_list_t *list) {
    // Check if pattern contains directory separator
    const char *last_slash = strrchr(pattern, '/');
    
    if (last_slash) {
        // Pattern has directory component
        char *dir_part = xmalloc(last_slash - pattern + 1);
        memcpy(dir_part, pattern, last_slash - pattern);
        dir_part[last_slash - pattern] = '\0';
        
        const char *file_part = last_slash + 1;
        
        if (has_glob_chars(file_part)) {
            expand_pattern_in_dir(file_part, dir_part, list);
        } else {
            // No glob in filename, just add as-is
            glob_list_add(list, pattern);
        }
        free(dir_part);
    } else {
        // Pattern is just filename, search in current directory
        if (has_glob_chars(pattern)) {
            expand_pattern_in_dir(pattern, ".", list);
        } else {
            // No glob chars, add as-is
            glob_list_add(list, pattern);
        }
    }
}

char **expand_glob_patterns(char **argv) {
    if (!argv || !argv[0]) return argv;
    
    glob_list_t expanded;
    glob_list_init(&expanded);
    
    bool any_expanded = false;
    bool any_matches = false;
    
    // Always keep command name (first argument)
    glob_list_add(&expanded, argv[0]);
    
    // Expand each argument starting from index 1
    for (int i = 1; argv[i]; i++) {
        if (has_glob_chars(argv[i])) {
            any_expanded = true;
            int before_count = expanded.count;
            expand_pattern(argv[i], &expanded);
            if (expanded.count > before_count) {
                any_matches = true;
            } else {
                // No matches for this pattern - keep pattern as-is (standard shell behavior)
                glob_list_add(&expanded, argv[i]);
            }
        } else {
            glob_list_add(&expanded, argv[i]);
        }
    }
    
    // If we expanded but got no matches, return original
    if (any_expanded && !any_matches && expanded.count == 1) {
        // Only command name, no arguments matched
        glob_list_free(&expanded);
        return argv;
    }
    
    // Add NULL terminator
    if (expanded.count >= expanded.capacity) {
        expanded.capacity = expanded.count + 1;
        expanded.items = realloc(expanded.items, expanded.capacity * sizeof(char*));
        if (!expanded.items) {
            perror("realloc");
            glob_list_free(&expanded);
            return argv;
        }
    }
    expanded.items[expanded.count] = NULL;
    
    return expanded.items;
}

void free_glob_expansion(char **argv) {
    if (!argv) return;
    
    // Check if this is a glob expansion (has more than original)
    // For simplicity, we'll free all if it looks like an expansion
    // In practice, we'd need to track which argv arrays were expanded
    // For now, we'll rely on the command structure to handle this
    (void)argv;
}

