#ifndef UNTESTABLE_2018_03_03_H
#define UNTESTABLE_2018_03_03_H

#include <stdio.h>

typedef struct {
        int line;
        const char *file, *func;
} SrcLoc;

#define HERE ((SrcLoc){__LINE__, __FILE__, __func__})

// Parse `faults` to activate any fault-injects defined there.  `faults` is a
// comma-separated list of fault names, you are not allowed to supply the same
// fault name more than once, or to use an unknown name.  The currently defined
// faults are
//
// unreadable-bangs: file_errnum will fake an I/O error if it sees '!'.
//
extern void set_injected_faults(const char *faults);

// Returns realloc(buf, n), except it reports failures to stderr and abort()s.
extern void *realloc_or_die(SrcLoc loc, void *buf, size_t n);

// Returns zero if there is no error on `fin`, otherwise a negative number
// There is an error on `fin` if `ferror(fin)` returns nonzero; there can also
// be errors depending on fault-injection settings and contents of buf[0:n].
extern int file_errnum(FILE *fin, void *buf, size_t n);

// Exactly the same as die(HERE, ...) except coverage doesn't count the line.
// Used this for code you expect to be unreachable.
#define DIE_LCOV_EXCL_LINE(...) die(HERE, __VA_ARGS__)

// Format zmsg to stderr, then abort().
extern int die(SrcLoc loc, const char *zmsg, ...)
    __attribute__((format(printf, 2, 3)));

// If (COND) is nonzero, print it and format zmsg to stderr, then abort().
// This check is unconditional, regardless of macros like NDEBUG etc.  COND is
// always evaluated exactly once.  But the zmsg... is only formated if COND is
// true.
#define DIE_IF(COND, ...)                                                      \
        do {                                                                   \
                if (COND)                                                      \
                        die_if(HERE, #COND, __VA_ARGS__);                      \
        } while (0)
extern int die_if(SrcLoc loc, const char *cond, const char *zmsg, ...)
    __attribute__((format(printf, 3, 4)));

// Print a message to stderr, then abort(). The message begins with the prefix
// "DBG: ", which the tests know to ignore.
#define DBG(...) dbg(HERE, __VA_ARGS__)
void dbg(SrcLoc loc, const char *zfmt, ...)
    __attribute__((format(printf, 2, 3)));

#endif // UNTESTABLE_2018_03_03_H
