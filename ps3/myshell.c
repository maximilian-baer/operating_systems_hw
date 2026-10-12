#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#define MAX_REDIR 3

struct redir {
    int fd;
    int flags;
    const char *path;
};

struct command {
    char *argv[256];
    int argc;
    struct redir redirs[MAX_REDIR];
    int nredir;
};

struct redir_op {
    const char *prefix;
    int fd;
    int flags;
}

redir_ops[] = {
    { "2>>", 2, O_WRONLY | O_CREAT | O_APPEND },
    { "2>",  2, O_WRONLY | O_CREAT | O_TRUNC  },
    { ">>",  1, O_WRONLY | O_CREAT | O_APPEND },
    { ">",   1, O_WRONLY | O_CREAT | O_TRUNC  },
    { "<",   0, O_RDONLY },
};

struct redir_op *match_redir(char *tok) {
    int n_ops = sizeof(redir_ops) / sizeof(redir_ops[0]);
    for (int i = 0; i < n_ops; i++) {
        if(strncmp(tok, redir_ops[i].prefix, strlen(redir_ops[i].prefix)) == 0) {
            return &redir_ops[i];
        }
    }

    return NULL;
}

int parse_line(char *line, struct command *cmd) {
    cmd->argc = 0;
    cmd->nredir = 0;

    // regard comments
    if(line[0] == '#') {
        return 0;
    }
    char *tok = strtok(line, " \t\n\r");
    while (tok != NULL) {

        struct redir_op *op = match_redir(tok);
        
        // No redirection required
        if(op == NULL) {
            if (cmd->argc >= 256) {
                fprintf(stderr, "Error: Too many arguments.\n");
                return -1;
            }
            cmd->argv[cmd->argc] = tok;
        
            cmd->argc = cmd->argc + 1;

        }else {
            if (cmd->nredir >= MAX_REDIR) {
                fprintf(stderr, "Error: too many redirections\n");
                return -1;
            }

            int prefix_len = strlen(op->prefix);
            char *path = tok + prefix_len;
            if (*path == '\0') {
                fprintf(stderr, "myshell: missing filename after '%s'\n", op->prefix);
                return -1;
            }
            int i = cmd->nredir;

            cmd->redirs[i].fd = op->fd;
            cmd->redirs[i].flags = op->flags;
            cmd->redirs[i].path = path;

            cmd->nredir = cmd->nredir + 1;
        }
        tok = strtok(NULL, " \t\n\r");
    }
    // only whitespaces
    if (cmd->argc == 0) {
        if (cmd->nredir > 0) {
            fprintf(stderr, "myshell: redirection without command\n");
            return -1;
        }
        return 0;
    }
    // Mark arg list's end with NULL
    cmd->argv[cmd->argc] = NULL;
    return 1;
}

static void dump_command(const struct command *cmd)
{
    fprintf(stderr, "[parse] argc=%d:", cmd->argc);
    for (int i = 0; i < cmd->argc; i++)
        fprintf(stderr, " <%s>", cmd->argv[i]);
    fputc('\n', stderr);
    for (int i = 0; i < cmd->nredir; i++) {
        fprintf(stderr, "[parse]   fd %d -> \"%s\" (%s)\n",
        cmd->redirs[i].fd, cmd->redirs[i].path,
        cmd->redirs[i].fd == 0 ? "read" :
        (cmd->redirs[i].flags & O_APPEND) ? "append" : "truncate");
    }
}



int main(int argc, char const *argv[])
{
    FILE *in = stdin;
    int last_status = 0;
    char *line = NULL;
    size_t cap = 0;

    if (argc > 2) {
        fprintf(stderr, "usage: %s [script]\n", argv[0]);
        return 1;
    }
    if (argc == 2) {
        // set FD_CLOEXEC flag
        if (!(in = fopen(argv[1], "re"))) {
            fprintf(stderr, "myshell: can't open script %s: %s\n", argv[1], strerror(errno));
            return 1;
        }
    }

    while(1) {
        errno = 0;
        if (getline(&line, &cap, in) == -1) {
            if (ferror(in)) {
                fprintf(stderr, "mysh: error reading input: %s\n", strerror(errno));
                // enables continuation of reading
                clearerr(in);
                continue;
            }
            break;
        }

        struct command cmd;
        int r = parse_line(line, &cmd);
        if (r < 0) {
            last_status = 1;
            continue;
        }else if (r == 0) {
            continue;
        }

        dump_command(&cmd);
    }
    
    // free getline()'s allocated memory
    free(line);
    if (in != stdin) {
        fclose(in);
    }
    fprintf(stderr, "end of file read, exiting shell with exit code %d\n", last_status);
    
    return last_status;
}
