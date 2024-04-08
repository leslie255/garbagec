#pragma once

#include <assert.h>
#include <execinfo.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef int64_t i64;
typedef int32_t i32;
typedef int16_t i16;
typedef int8_t i8;
typedef float f32;
typedef double f64;
typedef size_t usize;
typedef ssize_t isize;

#define nullptr ((void *)0)

#define PUT_ON_HEAP(X) memcpy(malloc(sizeof(X)), &X, sizeof(X))

void print_stacktrace() {
  void *callstack[128];
  int frames = backtrace(callstack, 128);
  char **strs = backtrace_symbols(callstack, frames);
  for (u32 i = 0; i < frames; i++) {
    fprintf(stderr, "%s\n", strs[i]);
  }
  free(strs);
}

/// Assert with stacktrace on failure.
#define ASSERT(COND)                                                                                                   \
  (((COND))                                                                                                            \
       ? 0                                                                                                             \
       : (fprintf(stderr, "[%s@%s:%d] Assertion failed: (" #COND ") == false\n", __FUNCTION__, __FILE__, __LINE__),    \
          print_stacktrace(), exit(1)))
/// Assert with stacktrace and print message on failure.
#define ASSERT_PRINT(COND, ...)                                                                                        \
  (((COND))                                                                                                            \
       ? 0                                                                                                             \
       : (fprintf(stderr, "[%s@%s:%d] Assertion failed: (" #COND ") == false\n", __FUNCTION__, __FILE__, __LINE__),    \
          fprintf(stderr, __VA_ARGS__), print_stacktrace(), exit(1)))

#ifdef DEBUG
/// Assert only in debug mode with stacktrace on failure.
#define DEBUG_ASSERT(COND)                                                                                             \
  (((COND))                                                                                                            \
       ? 0                                                                                                             \
       : (fprintf(stderr, "[%s@%s:%d] Assertion failed: (" #COND ") == false\n", __FUNCTION__, __FILE__, __LINE__),    \
          print_stacktrace(), exit(1)))
/// Assert only in debug mode with stacktrace and print message on failure.
#define DEBUG_ASSERT_PRINT(COND, ...)                                                                                  \
  (((COND))                                                                                                            \
       ? 0                                                                                                             \
       : (fprintf(stderr, "[%s@%s:%d] Assertion failed: (" #COND ") == false\n", __FUNCTION__, __FILE__, __LINE__),    \
          fprintf(stderr, __VA_ARGS__), print_stacktrace(), exit(1)))
#else
#define DEBUG_ASSERT(COND) 0
#define DEBUG_ASSERT_PRINT(COND, ...) 0
#endif

#define PANIC()                                                                                                        \
  (fprintf(stderr, "[%s@%s:%d] PANIC\n", __FUNCTION__, __FILE__, __LINE__), print_stacktrace(), exit(1))
#define PANIC_PRINT(...)                                                                                               \
  (fprintf(stderr, "[%s@%s:%d] PANIC\n", __FUNCTION__, __FILE__, __LINE__), fprintf(stderr, __VA_ARGS__),              \
   print_stacktrace(), exit(1))

/// Return `0` to the caller if value is `0`
#define TRY_NULL(X)                                                                                                    \
  {                                                                                                                    \
    if ((X) == 0) {                                                                                                    \
      return 0;                                                                                                        \
    }                                                                                                                  \
  }

#define PTR_CAST(TY, X) (*(TY *)&(X))

#define TODO() (printf("[%s@%s:%d] TODO\n", __FUNCTION__, __FILE__, __LINE__), exit(1))

__attribute__((always_inline)) static inline void *xalloc_(usize len) {
  void *p = malloc(len);
  ASSERT(p != nullptr);
  return p;
}

__attribute__((always_inline)) static inline void *xrealloc_(void *p, usize len) {
  DEBUG_ASSERT(p != nullptr);
  p = realloc(p, len);
  ASSERT(p != nullptr);
  return p;
}

__attribute__((always_inline)) static inline void xfree(void *p) {
  DEBUG_ASSERT(p != nullptr);
  free(p);
}

#define xalloc(TY, COUNT) xalloc_(sizeof(TY[(COUNT)]))
#define xrealloc(P, TY, COUNT) xrealloc_((P), sizeof(TY[(COUNT)]))
