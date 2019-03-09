#ifndef UNTESTABLE_2018_03_03_H
#define UNTESTABLE_2018_03_03_H

#include <stdio.h>

// Parse `faults` to activate any fault-injects defined there.  `faults` is a
// comma-separated list of fault names, you are not allowed to supply the same
// fault name more than once, or to use an unknown name.  The currently defined
// faults are
//
// unreadable-bangs: file_errnum will fake an I/O error if it sees '!'.
//
extern void set_injected_faults(const char *faults);

// Returns realloc(buf, n), except it reports failures to stderr and abort()s.
extern void *realloc_or_die(void *buf, size_t n);

// Returns zero if there is no error on `fin`, otherwise a negative number
// There is an error on `fin` if `ferror(fin)` returns nonzero; there can also
// be errors depending on fault-injection settings and contents of buf[0:n].
extern int file_errnum(FILE *fin, void *buf, size_t n);

#endif // UNTESTABLE_2018_03_03_H
