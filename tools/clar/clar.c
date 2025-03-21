/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <assert.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <stdarg.h>
#include <unistd.h>

/* required for sandboxing */
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
# include <windows.h>
# include <io.h>
# include <shellapi.h>
# include <direct.h>

# define _MAIN_CC __cdecl

# define stat(path, st) _stat(path, st)
# define mkdir(path, mode) _mkdir(path)
# define chdir(path) _chdir(path)
# define access(path, mode) _access(path, mode)
# define strdup(str) _strdup(str)

# ifndef __MINGW32__
#   pragma comment(lib, "shell32")
#   define strncpy(to, from, to_size) strncpy_s(to, to_size, from, _TRUNCATE)
#   define W_OK 02
#   define S_ISDIR(x) ((x & _S_IFDIR) != 0)
#   define snprint_eq(buf,sz,fmt,a,b) _snprintf_s(buf,sz,_TRUNCATE,fmt,a,b)
# else
#   define snprint_eq snprintf
# endif
  typedef struct _stat STAT_T;
#else

# ifdef UNITTEST_DEBUG
# include <signal.h>
# endif

# include <sys/wait.h> /* waitpid(2) */
# include <unistd.h>
# define _MAIN_CC
# define snprint_eq snprintf
  typedef struct stat STAT_T;
#endif

#include "clar.h"

#define CL_WITHIN(n, min, max) ((n) >= (min) && (n) <= (max))

bool clar_expecting_passert = false;
bool clar_passert_occurred = false;
jmp_buf clar_passert_jmp_buf;

static void fs_rm(const char *_source);
static void fs_copy(const char *_source, const char *dest);

static const char *
fixture_path(const char *base, const char *fixture_name);

struct clar_error {
  const char *test;
  int test_number;
  const char *suite;
  const char *file;
  int line_number;
  const char *error_msg;
  char *description;

  struct clar_error *next;
};

static struct {
  const char *active_test;
  const char *active_suite;

  int suite_errors;
  int total_errors;

  int test_count;

  int report_errors_only;
  int exit_on_error;

  struct clar_error *errors;
  struct clar_error *last_error;

  void (*local_cleanup)(void *);
  void *local_cleanup_payload;

  jmp_buf trampoline;
  int trampoline_enabled;
} _clar;

struct clar_func {
  const char *name;
  void (*ptr)(void);
};

struct clar_suite {
  int index;
  const char *name;
  struct clar_func initialize;
  struct clar_func cleanup;
  const char **categories;
  const struct clar_func *tests;
  size_t test_count;
};

/* From clar_print_*.c */
static void clar_print_init(int test_count, int suite_count, const char *suite_names);
static void clar_print_shutdown(int test_count, int suite_count, int error_count);
static void clar_print_error(int num, const struct clar_error *error);
static void clar_print_ontest(const char *test_name, int test_number, int failed);
static void clar_print_onsuite(const char *suite_name, int suite_index);
static void clar_print_onabort(const char *msg, ...);

/* From clar_sandbox.c */
static void clar_unsandbox(void);
static int clar_sandbox(void);

/* From clar_mock.c */
static void clar_mock_reset(void);
static void clar_mock_cleanup(void);

/* From clar_categorize.c */
static int clar_category_is_suite_enabled(const struct clar_suite *);
static void clar_category_enable(const char *category);
static void clar_category_enable_all(size_t, const struct clar_suite *);
static void clar_category_print_enabled(const char *prefix);

/* Event callback overrides */
${clar_event_overrides}

/* Autogenerated test data by clar */
${clar_callbacks}

${clar_categories}

static const struct clar_suite _clar_suites[] = {
  ${clar_suites}
};

static size_t _clar_suite_count = ${clar_suite_count};
static size_t _clar_callback_count = ${clar_callback_count};

/* Core test functions */
static void
clar_report_errors(void)
{
  int i = 1;
  struct clar_error *error, *next;

  error = _clar.errors;
  while (error != NULL) {
    next = error->next;
    clar_print_error(i++, error);
    free(error->description);
    free(error);
    error = next;
  }

  _clar.errors = _clar.last_error = NULL;
}

static void
clar_run_test(
  const struct clar_func *test,
  const struct clar_func *initialize,
  const struct clar_func *cleanup)
{
  int error_st = _clar.suite_errors;

  clar_on_test();
  _clar.trampoline_enabled = 1;

  clar_passert_occurred = false;
  clar_expecting_passert = false;
  if (setjmp(_clar.trampoline) == 0) {
    if (initialize->ptr != NULL)
      initialize->ptr();

    printf("\n#------------------------------------------------------------------------");
    printf("\n# Running test: %s...\n", test->name);
    test->ptr();
  }

  _clar.trampoline_enabled = 0;

  if (_clar.local_cleanup != NULL)
    _clar.local_cleanup(_clar.local_cleanup_payload);

  if (cleanup->ptr != NULL)
    cleanup->ptr();

  _clar.test_count++;

  /* remove any local-set cleanup methods */
  _clar.local_cleanup = NULL;
  _clar.local_cleanup_payload = NULL;

  if (_clar.report_errors_only)
    clar_report_errors();
  else
    clar_print_ontest(
      test->name,
      _clar.test_count,
      (_clar.suite_errors > error_st)
      );
}

static void
clar_run_suite(const struct clar_suite *suite)
{
  const struct clar_func *test = suite->tests;
  size_t i;

  if (!clar_category_is_suite_enabled(suite))
    return;

  if (_clar.exit_on_error && _clar.total_errors)
    return;

  if (!_clar.report_errors_only)
    clar_print_onsuite(suite->name, suite->index);
  clar_on_suite();

  _clar.active_suite = suite->name;
  _clar.suite_errors = 0;

  for (i = 0; i < suite->test_count; ++i) {
    _clar.active_test = test[i].name;
    clar_run_test(&test[i], &suite->initialize, &suite->cleanup);

    if (_clar.exit_on_error && _clar.total_errors)
      return;
  }
}

#if 0 /* temporarily disabled */
static void
clar_run_single(const struct clar_func *test,
  const struct clar_suite *suite)
{
  _clar.suite_errors = 0;
  _clar.active_suite = suite->name;
  _clar.active_test = test->name;

  clar_run_test(test, &suite->initialize, &suite->cleanup);
}
#endif

static void
clar_usage(const char *arg)
{
  printf("Usage: %s [options]\n\n", arg);
  printf("Options:\n");
  printf("  -sXX\t\tRun only the suite number or name XX\n");
  printf("  -i<name>\tInclude category <name> tests\n");
  printf("  -q  \t\tOnly report tests that had an error\n");
  printf("  -Q  \t\tQuit as soon as a test fails\n");
  printf("  -l  \t\tPrint suite, category, and test names\n");
  printf("  -tXX\t\tRun a specifc test by name\n");
  exit(-1);
}

static void
clar_parse_args(int argc, char **argv)
{
  int i;

  for (i = 1; i < argc; ++i) {
    char *argument = argv[i];

    if (argument[0] != '-')
      clar_usage(argv[0]);

    switch (argument[1]) {
    case 's': { /* given suite number, name, or prefix */
      int num = 0, offset = (argument[2] == '=') ? 3 : 2;
      int len = 0, is_num = 1, has_colon = 0, j;

      for (argument += offset; *argument; ++argument) {
        len++;
        if (*argument >= '0' && *argument <= '9')
          num = (num * 10) + (*argument - '0');
        else {
          is_num = 0;
          if (*argument == ':')
            has_colon = 1;
        }
      }

      argument = argv[i] + offset;

      if (!len)
        clar_usage(argv[0]);
      else if (is_num) {
        if ((size_t)num >= _clar_suite_count) {
          clar_print_onabort("Suite number %d does not exist.\n", num);
          exit(-1);
        }
        clar_run_suite(&_clar_suites[num]);
      }
      else if (!has_colon || argument[-1] == ':') {
        for (j = 0; j < (int)_clar_suite_count; ++j)
          if (strncmp(argument, _clar_suites[j].name, len) == 0)
            clar_run_suite(&_clar_suites[j]);
      }
      else {
        for (j = 0; j < (int)_clar_suite_count; ++j)
          if (strcmp(argument, _clar_suites[j].name) == 0) {
            clar_run_suite(&_clar_suites[j]);
            break;
          }
      }
      if (_clar.active_suite == NULL) {
        clar_print_onabort("No suite matching '%s' found.\n", argument);
        exit(-1);
      }
      break;
    }

    // Run a particular test only. This code was added on top of base clar.
    case 't': {
      int offset = (argument[2] == '=') ? 3 : 2;
      char *test_name = argument + offset;
      const struct clar_suite *suite = &_clar_suites[0];

      const struct clar_func *test_funcs = suite->tests;
      const struct clar_func *test = NULL;
      for (int i = 0; i < suite->test_count; i++) {
        if (!strcmp(test_funcs[i].name, test_name)) {
          test = &test_funcs[i];
          break;
        }
      }

      if (test == NULL) {
        clar_print_onabort("No test named '%s'.\n", argument);
        exit(-1);
      }

      _clar.active_suite = suite->name;
      _clar.suite_errors = 0;
      _clar.active_test = test->name;
      clar_run_test(test, &suite->initialize, &suite->cleanup);
      break;
    }


    case 'q':
      _clar.report_errors_only = 1;
      break;

    case 'Q':
      _clar.exit_on_error = 1;
      break;

    case 'i': {
      int offset = (argument[2] == '=') ? 3 : 2;
      if (strcasecmp("all", argument + offset) == 0)
        clar_category_enable_all(_clar_suite_count, _clar_suites);
      else
        clar_category_enable(argument + offset);
      break;
    }

    case 'l': {
      size_t j;
      printf("Test suites (use -s<name> to run just one):\n");
      for (j = 0; j < _clar_suite_count; ++j)
        printf(" %3d: %s\n", (int)j, _clar_suites[j].name);

      printf("\nCategories (use -i<category> to include):\n");
      clar_category_enable_all(_clar_suite_count, _clar_suites);
      clar_category_print_enabled(" - ");

      printf("\nTest names (use -t<name> to run just one):\n");
      for (j = 0; j < _clar_suite_count; ++j) {
        const struct clar_suite *suite = &_clar_suites[j];
        for (int i = 0; i < suite->test_count; i++) {
          printf("   %s\n", suite->tests[i].name);
        }
      }
      printf("\n");

      exit(0);
    }

    default:
      clar_usage(argv[0]);
    }
  }
}

static int
clar_test(int argc, char **argv)
{
  clar_print_init(
    (int)_clar_callback_count,
    (int)_clar_suite_count,
    ""
  );

  if (clar_sandbox() < 0) {
    clar_print_onabort("Failed to sandbox the test runner.\n");
    exit(-1);
  }

  clar_mock_reset();

  clar_on_init();

  if (argc > 1)
    clar_parse_args(argc, argv);

  if (_clar.active_suite == NULL) {
    size_t i;
    for (i = 0; i < _clar_suite_count; ++i)
      clar_run_suite(&_clar_suites[i]);
  }

  clar_print_shutdown(
    _clar.test_count,
    (int)_clar_suite_count,
    _clar.total_errors
  );

  clar_on_shutdown();

  clar_mock_cleanup();

  clar_unsandbox();
  return _clar.total_errors;
}

char *strdup(const char *s1);

void
clar__assert(
  int condition,
  const char *file,
  int line,
  const char *error_msg,
  const char *description,
  int should_abort)
{
  struct clar_error *error;

  if (condition)
    return;

#ifndef _WIN32
#ifdef UNITTEST_DEBUG
  // Break in debugger:
  raise(SIGINT);
#endif
#endif

  error = calloc(1, sizeof(struct clar_error));

  if (_clar.errors == NULL)
    _clar.errors = error;

  if (_clar.last_error != NULL)
    _clar.last_error->next = error;

  _clar.last_error = error;

  error->test = _clar.active_test;
  error->test_number = _clar.test_count;
  error->suite = _clar.active_suite;
  error->file = file;
  error->line_number = line;
  error->error_msg = error_msg;

  if (description != NULL)
    error->description = strdup(description);

  _clar.suite_errors++;
  _clar.total_errors++;

  if (should_abort) {
    if (!_clar.trampoline_enabled) {
      clar_print_onabort(
        "Fatal error: a cleanup method raised an exception.");
      clar_report_errors();
      exit(-1);
    }

    longjmp(_clar.trampoline, -1);
  }
}

void clar__assert_equal_s(
  const char *s1,
  const char *s2,
  const char *file,
  int line,
  const char *err,
  int should_abort)
{
  int match = (s1 == NULL || s2 == NULL) ? (s1 == s2) : (strcmp(s1, s2) == 0);

  if (!match) {
    char buf[4096];
    snprint_eq(buf, 4096, "'%s' != '%s'", s1, s2);
    clar__assert(0, file, line, err, buf, should_abort);
  }
}

void clar__assert_equal_i(
  int i1,
  int i2,
  const char *file,
  int line,
  const char *err,
  int should_abort)
{
  if (i1 != i2) {
    char buf[128];
    snprint_eq(buf, 128, "%d != %d", i1, i2);
    clar__assert(0, file, line, err, buf, should_abort);
  }
}


void clar__assert_equal_d(
  double d1,
  double d2,
  const char *file,
  int line,
  const char *err,
  int should_abort)
{
  if (d1 != d2) {
    char buf[128];
    snprint_eq(buf, 128, "%f != %f", d1, d2);
    clar__assert(0, file, line, err, buf, should_abort);
  }
}

void clar__assert_within(
  int n,
  int min,
  int max,
  const char *file,
  int line,
  const char *err,
  int should_abort)
{
  if (!CL_WITHIN(n, min, max)) {
    char buf[256];
    snprint_eq(buf, 256, "%d not within [%d, %d]", n, min, max);
    clar__assert(0, file, line, err, buf, should_abort);
  }
}

void clar__assert_near(
  int i1,
  int i2,
  int abs_err,
  const char *file,
  int line,
  const char *err,
  int should_abort)
{
  if (abs(i1 - i2) > abs_err) {
    char buf[256];
    snprint_eq(buf, 256, "Difference between %d and %d exceeds %d", i1, i2, abs_err);
    clar__assert(0, file, line, err, buf, should_abort);
  }
}

void clar__assert_cmp_i(
  int i1,
  int i2,
  ClarCmpOp op,
  const char *file,
  int line,
  const char *err,
  int should_abort)
{
  char *fmt;
  bool rv = false;
  switch (op) {
    case ClarCmpOp_EQ:
      fmt = "NOT ==";
      rv = (i1 == i2);
      break;
    case ClarCmpOp_LE:
      rv = (i1 <= i2);
      fmt = "NOT <=";
      break;
    case ClarCmpOp_LT:
      rv = (i1 < i2);
      fmt = "NOT <";
      break;
    case ClarCmpOp_GE:
      rv = (i1 >= i2);
      fmt = "NOT >=";
      break;
    case ClarCmpOp_GT:
      rv = (i1 > i2);
      fmt = "NOT >";
      break;
    case ClarCmpOp_NE:
      rv = (i1 != i2);
      fmt = "NOT !=";
      break;
  }
  if (!rv) {
    char buf[256] = {};
    snprintf(buf, 256, "%d  %s  %d", i1, fmt, i2);
    clar__assert(0, file, line, err, buf, should_abort);
  }
}


// Shamelessly stolen from
// http://stackoverflow.com/questions/7775991/how-to-get-hexdump-of-a-structure-data
void hex_dump(char *desc, void *addr, int len) {
  int i;
  unsigned char buff[17];
  unsigned char *pc = (unsigned char*)addr;

  // Output description if given.
  if (desc != NULL) {
    printf("%s:\n", desc);
  }

  // Process every byte in the data.
  for (i = 0; i < len; i++) {
    // Multiple of 16 means new line (with line offset).

    if ((i % 16) == 0) {
      // Just don't print ASCII for the zeroth line.
      if (i != 0) {
        printf("  %s\n", buff);
      }

      // Output the offset.
      printf("  %04x ", i);
    }

    // Now the hex code for the specific character.
    printf(" %02x", pc[i]);

    // And store a printable ASCII character for later.
    if ((pc[i] < 0x20) || (pc[i] > 0x7e)) {
      buff[i % 16] = '.';
    } else {
      buff[i % 16] = pc[i];
    }
    buff[(i % 16) + 1] = '\0';
  }

  // Pad out last line if not exactly 16 characters.
  while ((i % 16) != 0) {
    printf("   ");
    i++;
  }

  // And print the final ASCII bit.
  printf("  %s\n", buff);
}

void clar__assert_equal_m(
  uint8_t *m1,
  uint8_t *m2,
  int n,
  const char *file,
  int line,
  const char *err,
  int should_abort)
{
  if (m1 == m2 || n == 0) {
    return;
  }
  if (m1 == NULL || m2 == NULL) {
    char buf[4096];
    snprint_eq(buf, 4096, "'%p' != '%p'", m1, m2);
    clar__assert(0, file, line, err, buf, should_abort);
  }

  for (int offset = 0; offset < n; offset++) {
    if (m1[offset] == m2[offset]) {
      continue;
    }
    int len = 4096;
    int off = 0;
    char buf[len];
    off += snprint_eq(buf + off, len - off, "Mismatch at offset %d. Look above for diff.\n", offset);
    hex_dump("m1:", m1, n);
    hex_dump("m2:", m2, n);

    fflush(stdout);
    clar__assert(0, file, line, err, buf, should_abort);
  }
}

void cl_set_cleanup(void (*cleanup)(void *), void *opaque)
{
  _clar.local_cleanup = cleanup;
  _clar.local_cleanup_payload = opaque;
}

${clar_modules}

int _MAIN_CC main(int argc, char *argv[])
{
  return clar_test(argc, argv);
}
