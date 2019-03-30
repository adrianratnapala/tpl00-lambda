#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "untestable.h"

static bool fault_unreadable_bangs = false;
static const char *dbg_log_list = NULL;

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

// Parse `faults` to activate any fault-injects defined there.  `faults` is a
// comma-separated list of fault names, you are not allowed to supply the same
// fault name more than once, or to use an unknown name.  The currently defined
// faults are
//
// unreadable-bangs: file_errnum will fake an I/O error if it sees '!'.
static void set_injected_faults(const char *faults)
{
        if (!faults) {
                return;
        }
        if (!strcmp(faults, "unreadable-bangs")) {
                fault_unreadable_bangs = true;
        }
}

void init_debugging(void)
{
        set_injected_faults(secure_getenv("INJECTED_FAULTS"));
        dbg_log_list = secure_getenv("DEBUG");
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

static bool ignore_dbg(SrcLoc loc)
{
        if (!dbg_log_list)
                return true;

        // LCOV_EXCL_START
        size_t n = strlen(dbg_log_list);
        if (n == 1 && dbg_log_list[0] == '*')
                return false;

        return 0 != strncmp(loc.file, dbg_log_list, n);
        // LCOV_EXCL_STOP
}

// LCOV_EXCL_START
static void dbg_va(SrcLoc loc, const char *zfmt, va_list va)
{
        fprintf(stderr, "DBG: %s:%d: in `%s`: ", loc.file, loc.line, loc.func);
        vfprintf(stderr, zfmt, va);
        fputc('\n', stderr);
        fflush(stderr);
        va_end(va);
}

void dbg(SrcLoc loc, const char *zfmt, ...)
{
        if (ignore_dbg(loc)) {
                return;
        }
        va_list va;
        va_start(va, zfmt);
        return dbg_va(loc, zfmt, va);
}

// LCOV_EXCL_STOP
