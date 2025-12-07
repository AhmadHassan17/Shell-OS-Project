#define _GNU_SOURCE
#include "shell.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static struct termios saved_termios;
static bool raw_mode = false;

static void enable_raw_mode(void) {
    if (raw_mode) return;
    if (!isatty(STDIN_FILENO)) return;
    
    if (tcgetattr(STDIN_FILENO, &saved_termios) < 0)
        return;
    
    struct termios raw = saved_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;  // Wait for at least 1 byte (blocking)
    raw.c_cc[VTIME] = 0; // No timeout
    
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0)
        return;
    
    raw_mode = true;
}

static void disable_raw_mode(void) {
    if (!raw_mode) return;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_termios);
    raw_mode = false;
}

static int read_key(void) {
    int c = getchar();
    if (c == EOF) {
        if (feof(stdin)) return EOF;
        // Check for real errors
        if (errno != 0 && errno != EINTR) {
            return EOF;
        }
        // EINTR means interrupted by signal, try again
        return read_key();
    }
    return c;
}

ssize_t read_line_with_history(char **lineptr, size_t *n) {
    if (!isatty(STDIN_FILENO)) {
        // Not a terminal, use regular getline
        return getline(lineptr, n, stdin);
    }
    
    enable_raw_mode();
    
    char *line = *lineptr ? *lineptr : NULL;
    size_t cap = *n ? *n : 0;
    size_t len = 0;
    size_t cursor = 0;
    int completion_list_pos = -1; // -1 means not in completion mode
    
    history_reset_browse();
    
    while (1) {
        int c = read_key();
        if (c == EOF || c == '\n' || c == '\r') {
            disable_raw_mode();
            if (c == EOF && len == 0) {
                *lineptr = line;
                *n = cap;
                return -1;
            }
            putchar('\n');
            fflush(stdout);
            if (len + 1 > cap) {
                size_t ncap = cap ? cap * 2 : 64;
                char *nb = realloc(line, ncap);
                if (!nb) {
                    perror("realloc");
                    disable_raw_mode();
                    return -1;
                }
                line = nb;
                cap = ncap;
            }
            line[len] = '\0';
            *lineptr = line;
            *n = cap;
            return len;
        }
        
        if (c == 27) { // ESC
            int c2 = read_key();
            if (c2 == '[') {
                int c3 = read_key();
                if (c3 == 'A') { // Up arrow
                    const char *hist = history_get(1);
                    if (hist) {
                        // Clear current line
                        for (size_t i = 0; i < cursor; i++)
                            printf("\b \b");
                        for (size_t i = cursor; i < len; i++)
                            printf(" \b");
                        fflush(stdout);
                        
                        len = strlen(hist);
                        cursor = len;
                        if (len + 1 > cap) {
                            size_t ncap = len + 1;
                            char *nb = realloc(line, ncap);
                            if (!nb) {
                                perror("realloc");
                                disable_raw_mode();
                                return -1;
                            }
                            line = nb;
                            cap = ncap;
                        }
                        memcpy(line, hist, len);
                        line[len] = '\0';
                        printf("%s", line);
                        fflush(stdout);
                    }
                    continue;
                } else if (c3 == 'B') { // Down arrow
                    const char *hist = history_get(-1);
                    if (hist) {
                        // Clear current line
                        for (size_t i = 0; i < cursor; i++)
                            printf("\b \b");
                        for (size_t i = cursor; i < len; i++)
                            printf(" \b");
                        fflush(stdout);
                        
                        len = strlen(hist);
                        cursor = len;
                        if (len + 1 > cap) {
                            size_t ncap = len + 1;
                            char *nb = realloc(line, ncap);
                            if (!nb) {
                                perror("realloc");
                                disable_raw_mode();
                                return -1;
                            }
                            line = nb;
                            cap = ncap;
                        }
                        memcpy(line, hist, len);
                        line[len] = '\0';
                        printf("%s", line);
                        fflush(stdout);
                    } else {
                        // Clear current line
                        for (size_t i = 0; i < cursor; i++)
                            printf("\b \b");
                        for (size_t i = cursor; i < len; i++)
                            printf(" \b");
                        fflush(stdout);
                        len = 0;
                        cursor = 0;
                        if (line) line[0] = '\0';
                    }
                    continue;
                } else if (c3 == 'C') { // Right arrow
                    if (cursor < len) {
                        cursor++;
                        printf("\033[C");
                        fflush(stdout);
                    }
                    continue;
                } else if (c3 == 'D') { // Left arrow
                    if (cursor > 0) {
                        cursor--;
                        printf("\033[D");
                        fflush(stdout);
                    }
                    continue;
                }
            }
            // Unknown escape sequence, ignore
            continue;
        }
        
        if (c == '\t') { // Tab - completion
            // Find current word being completed
            size_t word_start = cursor;
            while (word_start > 0 && line[word_start - 1] != ' ' && line[word_start - 1] != '\t' &&
                   line[word_start - 1] != '|' && line[word_start - 1] != ';' && line[word_start - 1] != '&' &&
                   line[word_start - 1] != '<' && line[word_start - 1] != '>') {
                word_start--;
            }
            
            // Build current line string for completion
            if (len + 1 > cap) {
                size_t ncap = cap ? cap * 2 : 64;
                char *nb = realloc(line, ncap);
                if (!nb) {
                    perror("realloc");
                    disable_raw_mode();
                    return -1;
                }
                line = nb;
                cap = ncap;
            }
            line[len] = '\0';
            
            // Check if we're about to show a list (first time)
            bool will_show_list = (completion_list_pos < 0);
            
            // Disable raw mode before calling complete_input if we'll show a list
            if (will_show_list) {
                disable_raw_mode();
            }
            
            char *completion = NULL;
            int result = complete_input(line, cursor, &completion, &completion_list_pos);
            
            // Re-enable raw mode after printing list
            if (will_show_list && result > 1) {
                // List was printed, now redraw prompt and current line
                char *prompt = get_prompt();
                fputs(prompt, stdout);
                free(prompt);
                
                // Redraw the current line
                for (size_t i = 0; i < len; i++) {
                    fputc(line[i], stdout);
                }
                fflush(stdout);
                
                // Re-enable raw mode
                enable_raw_mode();
            }
            
            if (result > 0 && completion) {
                
                // Clear current word
                for (size_t i = word_start; i < cursor; i++) {
                    printf("\b \b");
                }
                
                // Replace with completion
                size_t comp_len = strlen(completion);
                size_t word_len = cursor - word_start;
                
                // Adjust buffer if needed
                if (len - word_len + comp_len + 1 > cap) {
                    size_t ncap = len - word_len + comp_len + 1;
                    char *nb = realloc(line, ncap);
                    if (!nb) {
                        free(completion);
                        continue;
                    }
                    line = nb;
                    cap = ncap;
                }
                
                // Replace word in buffer
                memmove(line + word_start + comp_len, line + cursor, len - cursor);
                memcpy(line + word_start, completion, comp_len);
                len = len - word_len + comp_len;
                cursor = word_start + comp_len;
                line[len] = '\0';
                
                // Print completion
                printf("%s", completion);
                
                // Redraw rest of line if needed
                if (cursor < len) {
                    for (size_t i = cursor; i < len; i++) {
                        printf("%c", line[i]);
                    }
                    // Move cursor back to end of completion
                    for (size_t i = 0; i < len - cursor; i++) {
                        printf("\b");
                    }
                }
                fflush(stdout);
                
                free(completion);
            } else if (result == 0) {
                // No completion - beep or do nothing
                printf("\a");
                fflush(stdout);
            }
            // If result > 1, list was already printed
            continue;
        }
        
        if (c == 127 || c == '\b') { // Backspace
            completion_list_pos = -1; // Reset completion state
            if (cursor > 0) {
                // Remove character at cursor-1
                memmove(line + cursor - 1, line + cursor, len - cursor);
                len--;
                cursor--;
                
                // Move cursor back and clear to end of line
                printf("\b");
                // Clear from cursor to end of line
                printf("\033[K");
                
                // Redraw remaining characters
                for (size_t i = cursor; i < len; i++) {
                    printf("%c", line[i]);
                }
                
                // Move cursor back to correct position
                if (cursor < len) {
                    for (size_t i = 0; i < len - cursor; i++) {
                        printf("\b");
                    }
                }
                fflush(stdout);
            }
            continue;
        }
        
        if (c < 32 || c > 126) continue; // Skip control chars except handled ones
        
        // Reset completion state when typing
        completion_list_pos = -1;
        
        // Insert character
        if (len + 1 > cap) {
            size_t ncap = cap ? cap * 2 : 64;
            char *nb = realloc(line, ncap);
            if (!nb) {
                perror("realloc");
                disable_raw_mode();
                return -1;
            }
            line = nb;
            cap = ncap;
        }
        
        if (cursor < len) {
            memmove(line + cursor + 1, line + cursor, len - cursor);
        }
        line[cursor] = c;
        len++;
        cursor++;
        
        // Print from cursor to end
        printf("%c", c);
        if (cursor < len) {
            for (size_t i = cursor; i < len; i++)
                printf("%c", line[i]);
            // Move cursor back
            for (size_t i = 0; i < len - cursor; i++)
                printf("\b");
        }
        fflush(stdout);
    }
}

void input_cleanup(void) {
    disable_raw_mode();
}

