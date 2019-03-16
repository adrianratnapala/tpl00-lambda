#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <getopt.h>

#include "lambda.h"
#include "untestable.h"

typedef struct {
        // Just test code for reading sources.  Read the input and
        // write it, and it's length to stdout.
        bool test_source_read;
        struct {
                bool unparse;
        } actions;
} LambdaConfig;

static LambdaConfig parse_argv_or_die(int argc, char *const *argv)
{
        LambdaConfig conf = {0};
        enum Opt
        {
                OPT_DONE = -1,
                OPT_BAD = '?',
                // OPT_DEFAULT = ':',
                OPT_TEST_SOURCE_READ = 1000,
                OPT_ACT_UNPARSE,
        };
        enum
        {
                HAS_NO_ARG,
                HAS_ARG,
        };
        static struct option longopts[] = {
            {"test-source-read", HAS_NO_ARG, NULL, OPT_TEST_SOURCE_READ},
            {"unparse", HAS_NO_ARG, NULL, OPT_ACT_UNPARSE},
            {0},
        };

        unsigned nacts = 0;
        for (;;) {
                enum Opt c = getopt_long(argc, argv, "", longopts, NULL);
                switch (c) {
                case OPT_TEST_SOURCE_READ:
                        conf.test_source_read = true;
                        continue;
                case OPT_ACT_UNPARSE:
                        conf.actions.unparse = true;
                        nacts++;
                case OPT_DONE:
                        goto end;
                case OPT_BAD: /* deliberate fallthrough */;
                default:
                        // Should have already printed more specific message.
                        fprintf(stderr, "Error parsing command line\n");
                        fflush(stderr);
                        exit(1);
                }
        }
end:

        if (nacts && conf.test_source_read) {
                fprintf(stderr, "--test-source-read means read the then exit, "
                                "it cannot be used along with actions:\n");
                if (conf.actions.unparse) {
                        fprintf(stderr, "    --unparse\n");
                }
                fflush(stderr);
                exit(1);
        }

        if (!nacts) {
                nacts++;
                conf.actions.unparse = true;
        }

        return conf;
}

static int read_whole_file(FILE *fin, char **obuf, size_t *osize)
{
        size_t used = 0;
        size_t alloced = 8192;
        int ern = 0;
        char *buf = realloc_or_die(HERE, NULL, alloced);

        size_t n = 0;
        char *ptr = 0;
        if (!feof(fin))
                do {
                        ptr = buf + used;
                        size_t rem = alloced - used;
                        errno = 0;
                        if (rem < 1024) {
                                buf = realloc_or_die(HERE, buf, (alloced *= 2));
                                continue;
                        }
                        n = fread(ptr, 1, rem - 1, fin);
                        used += n;
                } while (!(ern = file_errnum(fin, ptr, n)) && !feof(fin));

        buf[used] = 0;
        *obuf = realloc_or_die(HERE, buf, used + 1);
        *osize = used;
        return ern;
}

static char *read_stdin_or_exit(const LambdaConfig *config)
{
        size_t size;
        char *buf;
        int nerr = read_whole_file(stdin, &buf, &size);

        if (nerr < 0) {
                fprintf(stderr, "Error reading STDIN: %s\n", strerror(-nerr));
                free(buf);
                exit(1);
        }
        assert(buf);
        assert(strlen(buf) == size);

        if (config->test_source_read) {
                printf("%lu %s\n", size, buf);
                free(buf);
                exit(0);
        }

        return buf;
}

static int do_actions(const LambdaConfig *conf, const Ast *ast)
{
        int nerr = 0;
        if (conf->actions.unparse) {
                nerr += act_unparse(stdout, ast);
        }
        return 0;
}

int main(int argc, char *const *argv)
{
        set_injected_faults(secure_getenv("INJECTED_FAULTS"));
        LambdaConfig config = parse_argv_or_die(argc, argv);

        char *zsrc = read_stdin_or_exit(&config);

        Ast *ast = parse("STDIN", zsrc);
        int nerr = report_syntax_errors(stderr, ast);
        if (!nerr) {
                nerr = do_actions(&config, ast);
        }

        delete_ast(ast);
        free(zsrc);
        return nerr ? 1 : 0;
}
