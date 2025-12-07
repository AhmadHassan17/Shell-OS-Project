#include "shell.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void free_redirs(redir_t *r) {
    while (r) {
        redir_t *next = r->next;
        free(r->filename);
        free(r);
        r = next;
    }
}

void free_command_list(command_t *cmd) {
    while (cmd) {
        command_t *next_seq = cmd->next_seq;
        command_t *p = cmd;
        while (p) {
            command_t *next_pipe = p->next_pipe;
            if (p->argv) {
                for (size_t i = 0; p->argv[i]; i++)
                    free(p->argv[i]);
                free(p->argv);
            }
            free_redirs(p->redirs);
            free(p);
            p = next_pipe;
        }
        cmd = next_seq;
    }
}

typedef struct vec {
    char **data;
    size_t len;
    size_t cap;
} vec_t;

static void vec_init(vec_t *v) {
    v->data = NULL;
    v->len = v->cap = 0;
}

static void vec_push(vec_t *v, char *s) {
    if (v->len + 1 > v->cap) {
        size_t ncap = v->cap ? v->cap * 2 : 8;
        char **nd = realloc(v->data, ncap * sizeof(char *));
        if (!nd) {
            perror("realloc");
            exit(1);
        }
        v->data = nd;
        v->cap = ncap;
    }
    v->data[v->len++] = s;
}

static char *parse_word(const char **ps) {
    const char *s = *ps;
    char *buf = NULL;
    size_t cap = 0, len = 0;

    while (*s) {
        if (isspace((unsigned char)*s) || *s == '|' || *s == '&' ||
            *s == ';' || *s == '<' || *s == '>') {
            break;
        }
        if (*s == '\\') {
            s++;
            if (*s == '\0') break;
            char c = *s++;
            if (len + 2 > cap) {
                size_t ncap = cap ? cap * 2 : 16;
                char *nb = realloc(buf, ncap);
                if (!nb) {
                    perror("realloc");
                    exit(1);
                }
                buf = nb;
                cap = ncap;
            }
            buf[len++] = c;
            continue;
        } else if (*s == '\'') {
            s++;
            while (*s && *s != '\'') {
                if (len + 2 > cap) {
                    size_t ncap = cap ? cap * 2 : 16;
                    char *nb = realloc(buf, ncap);
                    if (!nb) {
                        perror("realloc");
                        exit(1);
                    }
                    buf = nb;
                    cap = ncap;
                }
                buf[len++] = *s++;
            }
            if (*s == '\'') s++;
            continue;
        } else if (*s == '"') {
            s++;
            while (*s && *s != '"') {
                char c = *s++;
                if (c == '\\' && *s) {
                    char esc = *s++;
                    switch (esc) {
                        case 'n': c = '\n'; break;
                        case '"': c = '"'; break;
                        case '\\': c = '\\'; break;
                        default: c = esc; break;
                    }
                }
                if (len + 2 > cap) {
                    size_t ncap = cap ? cap * 2 : 16;
                    char *nb = realloc(buf, ncap);
                    if (!nb) {
                        perror("realloc");
                        exit(1);
                    }
                    buf = nb;
                    cap = ncap;
                }
                buf[len++] = c;
            }
            if (*s == '"') s++;
            continue;
        } else {
            if (len + 2 > cap) {
                size_t ncap = cap ? cap * 2 : 16;
                char *nb = realloc(buf, ncap);
                if (!nb) {
                    perror("realloc");
                    exit(1);
                }
                buf = nb;
                cap = ncap;
            }
            buf[len++] = *s++;
        }
    }
    if (!buf) return NULL;
    buf[len] = '\0';
    *ps = s;
    return buf;
}

static command_t *new_command(void) {
    command_t *c = calloc(1, sizeof(*c));
    if (!c) {
        perror("calloc");
        exit(1);
    }
    return c;
}

command_t *parse_line(const char *line) {
    const char *s = line;
    command_t *seq_head = NULL;
    command_t *seq_tail = NULL;

    while (*s) {
        while (isspace((unsigned char)*s))
            s++;
        if (!*s)
            break;

        command_t *first_in_pipeline = NULL;
        command_t *last_in_pipeline = NULL;

        for (;;) {
            command_t *cmd = new_command();
            vec_t args;
            vec_init(&args);

            while (*s && !strchr("|;&", *s)) {
                while (isspace((unsigned char)*s))
                    s++;
                if (!*s || strchr("|;&", *s))
                    break;
                if (*s == '<' || *s == '>') {
                    redir_type_t rtype;
                    if (*s == '<') {
                        rtype = REDIR_IN;
                        s++;
                    } else {
                        s++;
                        if (*s == '>') {
                            rtype = REDIR_APPEND;
                            s++;
                        } else {
                            rtype = REDIR_OUT;
                        }
                    }
                    while (isspace((unsigned char)*s))
                        s++;
                    char *fname = parse_word(&s);
                    if (!fname) {
                        fprintf(stderr, "syntax error: missing filename after redirection\n");
                        free_command_list(cmd);
                        free_command_list(first_in_pipeline);
                        free_command_list(seq_head);
                        return NULL;
                    }
                    redir_t *r = calloc(1, sizeof(*r));
                    if (!r) {
                        perror("calloc");
                        exit(1);
                    }
                    r->type = rtype;
                    r->filename = fname;
                    r->next = cmd->redirs;
                    cmd->redirs = r;
                    continue;
                } else {
                    char *w = parse_word(&s);
                    if (w)
                        vec_push(&args, w);
                }
            }

            if (args.len == 0) {
                free(cmd);
            } else {
                vec_push(&args, NULL);
                cmd->argv = args.data;

                if (!first_in_pipeline)
                    first_in_pipeline = cmd;
                else
                    last_in_pipeline->next_pipe = cmd;
                last_in_pipeline = cmd;
            }

            while (isspace((unsigned char)*s))
                s++;
            if (*s == '|') {
                s++;
                continue;
            }
            break;
        }

        if (!first_in_pipeline) {
            continue;
        }

        bool background = false;
        while (isspace((unsigned char)*s))
            s++;
        if (*s == '&') {
            background = true;
            s++;
        }
        while (isspace((unsigned char)*s))
            s++;
        if (*s == ';')
            s++;

        for (command_t *p = first_in_pipeline; p; p = p->next_pipe)
            p->background = background;

        if (!seq_head)
            seq_head = first_in_pipeline;
        else
            seq_tail->next_seq = first_in_pipeline;
        seq_tail = first_in_pipeline;
    }

    return seq_head;
}




