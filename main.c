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
        // Just test code for reading sources.  Read the input and write it,
        // and it's length to stdout.
        bool test_source_read;
} LambdaConfig;

static void parse_argv_or_die(int argc, char *const *argv, LambdaConfig *config)
{
        enum Opt
        {
                OPT_DONE = -1,
                OPT_BAD = '?',
                // OPT_DEFAULT = ':',
                OPT_TEST_SOURCE_READ = 1000,
        };
        enum
        {
                HAS_NO_ARG,
                HAS_ARG,
        };
        static struct option longopts[] = {
            {"test-source-read", HAS_NO_ARG, NULL, OPT_TEST_SOURCE_READ},
            {0},
        };

        for (;;) {
                enum Opt c = getopt_long(argc, argv, "", longopts, NULL);
                switch (c) {
                case OPT_TEST_SOURCE_READ:
                        config->test_source_read = true;
                        continue;
                case OPT_DONE:
                        return;
                case OPT_BAD: /* deliberate fallthrough */;
                default:
                        // Should have already printed more specific message.
                        fprintf(stderr, "Error parsing command line\n");
                        exit(1);
                }
        }
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

int main(int argc, char *const *argv)
{
        LambdaConfig config = {0};
        set_injected_faults(secure_getenv("INJECTED_FAULTS"));
        parse_argv_or_die(argc, argv, &config);

        size_t size;
        char *buf;
        int nerr = read_whole_file(stdin, &buf, &size);

        if (nerr < 0) {
                free(buf);
                fprintf(stderr, "Error reading STDIN: %s\n", strerror(-nerr));
                exit(1);
        }
        assert(buf);
        assert(strlen(buf) == size);

        if (config.test_source_read) {
                printf("%lu %s\n", size, buf);
                return 0;
        }

        return interpret(stdout, "STDIN", size, buf) ? 1 : 0;
}
