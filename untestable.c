#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "untestable.h"

static bool fault_unreadable_bangs = false;

void *realloc_or_die(SrcLoc loc, void *buf, size_t n)
{
        buf = realloc(buf, n);
        if (n && !buf) {
                abort(); // LCOV_EXCL_LINE
        }
        return buf;
}

static int check_unreadable_bangs(const void *buf, size_t n)
{
        if (!fault_unreadable_bangs)
                return 0;
        if (!memchr(buf, '!', n))
                return 0; // LCOV_EXCL_LINE
        return -EIO;
}

int file_errnum(FILE *fin, void *buf, size_t n)
{
        int ret = check_unreadable_bangs(buf, n);
        if (ret) {
                return ret;
        }
        if (!ferror(fin)) {
                return (errno = 0);
        }
        // LCOV_EXCL_START
        if (errno) {
                return -errno;
        }
        return -1000 * 1000;
        // LCOV_EXCL_STOP
}

void set_injected_faults(const char *faults)
{
        if (!faults) {
                return;
        }
        if (!strcmp(faults, "unreadable-bangs")) {
                fault_unreadable_bangs = true;
        }
}

// LCOV_EXCL_START
static int die_va(SrcLoc loc, const char *prefix, const char *zfmt, va_list va)
{
        fprintf(stderr, "%s:%d: error in `%s`", loc.file, loc.line, loc.func);
        if (prefix) {
                fprintf(stderr, " (%s)", prefix);
        }
        fputs(": ", stderr);
        vfprintf(stderr, zfmt, va);
        fputc('\n', stderr);
        fflush(stderr);

        va_end(va);
        abort();
        return -1;
}

int die(SrcLoc loc, const char *zfmt, ...)
{
        va_list va;
        va_start(va, zfmt);
        return die_va(loc, NULL, zfmt, va);
}

int die_if(SrcLoc loc, const char *cond, const char *zfmt, ...)
{
        va_list va;
        va_start(va, zfmt);
        return die_va(loc, cond, zfmt, va);
}
// LCOV_EXCL_STOP
