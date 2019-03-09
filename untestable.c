#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "untestable.h"

static bool fault_unreadable_bangs = false;

void *realloc_or_die(void *buf, size_t n)
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
