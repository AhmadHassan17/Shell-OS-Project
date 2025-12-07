// Global shell definitions and shared data structures

#ifndef SHELL_H
#define SHELL_H

#include <stdbool.h>
#include <stddef.h>

#include <sys/types.h>

typedef struct shell_state {
    int   last_status;
    bool  running;
} shell_state_t;

// From parser.c
typedef enum {
    TOK_WORD,
    TOK_PIPE,
    TOK_SEMI,
    TOK_IN,
    TOK_OUT,
    TOK_APPEND,
    TOK_AMP,
    TOK_END
} token_type_t;

typedef struct token {
    token_type_t type;
    char        *value;
} token_t;

typedef enum {
    REDIR_IN,
    REDIR_OUT,
    REDIR_APPEND
} redir_type_t;

typedef struct redir {
    redir_type_t type;
    char        *filename;
    struct redir *next;
} redir_t;

typedef struct command {
    char         **argv;       // NULL-terminated args
    redir_t      *redirs;      // linked list of redirections
    bool          background;  // ends with '&'
    struct command *next_pipe; // next command in pipeline
    struct command *next_seq;  // next command after ';'
} command_t;

// parser.c
command_t *parse_line(const char *line);
void       free_command_list(command_t *cmd);

// builtins.c
bool is_builtin(const char *name);
int  run_builtin(shell_state_t *sh, command_t *cmd);

// exec.c
int execute_commands(shell_state_t *sh, command_t *cmd);

// jobs.c
void jobs_init(void);
void jobs_add(pid_t pgid, const char *cmdline, bool background);
void jobs_reap(bool blocking);
void jobs_print(void);

// signals.c
void signals_init(void);

// util.c
char *xstrdup(const char *s);
void *xmalloc(size_t sz);
char *get_prompt(void);

// history.c
void history_init(void);
void history_add(const char *line);
const char *history_get(int direction);
void history_reset_browse(void);
void history_print(void);
void history_cleanup(void);

// aliases.c
void aliases_init(void);
void alias_set(const char *name, const char *value);
void alias_unset(const char *name);
const char *alias_get(const char *name);
void alias_print(const char *name);
char *alias_expand(const char *line);
void aliases_cleanup(void);

// input.c
ssize_t read_line_with_history(char **lineptr, size_t *n);
void input_cleanup(void);

// completion.c
int complete_input(const char *line, size_t cursor, char **completion, int *list_pos);

// glob.c
char **expand_glob_patterns(char **argv);
void free_glob_expansion(char **argv);

#endif // SHELL_H


