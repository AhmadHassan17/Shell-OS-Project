#define _GNU_SOURCE
#include "shell.h"

#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

void *xmalloc(size_t sz) {
    void *p = malloc(sz);
    if (!p) {
        perror("malloc");
        exit(1);
    }
    return p;
}

char *xstrdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) {
        perror("strdup");
        exit(1);
    }
    return d;
}

char *get_prompt(void) {
    static char buf[512];
    char host[128];
    char cwd[256];
    const char *user = NULL;

    struct passwd *pw = getpwuid(getuid());
    if (pw) user = pw->pw_name;
    if (!user) user = "user";

    if (gethostname(host, sizeof(host)) != 0) {
        strncpy(host, "host", sizeof(host));
        host[sizeof(host) - 1] = '\0';
    }
    if (!getcwd(cwd, sizeof(cwd))) {
        strncpy(cwd, "?", sizeof(cwd));
        cwd[sizeof(cwd) - 1] = '\0';
    }

    snprintf(buf, sizeof(buf), "%s@%s:%s$ ", user, host, cwd);
    return buf;
}


