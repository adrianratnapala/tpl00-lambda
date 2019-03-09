B ?= b

PY_TEST=py.test
GCOVR=gcovr

OPTFLAGS ?= -g -Werror
CFLAGS = -std=c11 $(OPTFLAGS) $(COVFLAGS) -Wall -Wno-parentheses
LDFLAGS= $(LDOPTFLAGS) $(COVFLAGS)
CLANG_FORMAT=clang-format

ifeq "$(COVERAGE)" "true"
COVFLAGS=-fprofile-arcs -ftest-coverage
endif

PROGS = $B/lambda

all: fmt progs

$B/lambda: \
        $B/lambda.o \
        $B/main.o \
        $B/untestable.o

$B/%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

coverage: test
	$(GCOVR) --fail-under-line 100

progs: dirs $(PROGS)

.PHONY: test
test: dirs $(PROGS)
	$(PY_TEST)

.PHONY: clean
clean:
	rm -f $(PROGS)
	rm -f *.gcov
	rm -rf "$B"

.PHONY: dirs
dirs:
	mkdir -p $B

main.o: lambda.h untestable.h
lambda.o: lambda.h

fmt:
	$(CLANG_FORMAT) -i *.c *.h
