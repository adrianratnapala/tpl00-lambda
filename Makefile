B ?= b

PY_TEST=py.test
GCOVR=gcovr

OPTFLAGS ?= -g -Werror
CFLAGS = -std=c11 $(OPTFLAGS) $(COVFLAGS) -Wall -Wno-parentheses
LDFLAGS= $(LDOPTFLAGS) $(COVFLAGS)
CLANG_FORMAT=clang-format

USE_VALGRIND?=no
COVERAGE?=no
TEST_MODE?=quick

ifeq "$(TEST_MODE)" "full"
COVERAGE=yes
USE_VALGRIND=yes
endif

ifeq "$(COVERAGE)" "yes"
COVFLAGS=-fprofile-arcs -ftest-coverage
endif

PROGS = $B/lambda

# `built` builds from source, but to avoid dependencies, it doesn't
# format source etc.
build: progs

# Like `build` but with additional goodies such as `clang-format`
all: fmt progs

$B/lambda: \
        $B/lambda.o \
        $B/main.o \
        $B/parse.o \
        $B/type.o \
        $B/untestable.o

$B/%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

coverage: test_without_coverage
	$(GCOVR) --fail-under-line 100

progs: dirs $(PROGS)

.PHONY: test_without_coverage
test_without_coverage: dirs $(PROGS)
	USE_VALGRIND=$(USE_VALGRIND) $(PY_TEST)

ifeq "$(COVERAGE)" "yes"
test: coverage
else
test: test_without_coverage
endif

.PHONY: clean
clean:
	rm -f $(PROGS)
	rm -f *.gcov
	rm -rf "$B"

.PHONY: dirs
dirs:
	mkdir -p $B

$B/lambda.o: lambda.h untestable.h
$B/main.o: lambda.h untestable.h
$B/parse.o: lambda.h untestable.h
$B/type.o: lambda.h untestable.h
$B/untestable.o: untestable.h


fmt:
	$(CLANG_FORMAT) -i *.c *.h
