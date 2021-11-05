#ifndef __TRACE_H
#define __TRACE_H

#include <stdio.h>
#include <stdbool.h>

/* #define RED     "31" */
/* #define GREEN   "32" */
/* #define YELLOW  "33" */
/* #define BLUE    "34" */
/* #define MAGENTA "35" */
/* #define CYAN    "36" */
/* #define BOLD     "1" */

#define T_ERROR             1
#define T_ERROR_MARKER "EE"
#define T_WARN              2
#define T_WARN_MARKER  "WW"
#define T_INFO              3
#define T_INFO_MARKER  "II"
#define T_GURU              4
#define T_GURU_MARKER  "GG"
#define T_ZEN               5
#define T_ZEN_MARKER   "ZZ"
#define T_MAX T_ZEN

#define T_DEFAULT_LVL   T_INFO
#define T_DEFAULT_OUT   stderr

#define T_TAGS   (sizeof(long) * 8) /* bitmask */
#define T_ALL    (~(long)0)
#define T_NONE   0
#define T_LEVELS (T_MAX + 1)
#define T_TAG(n) (1 << (n))

#ifdef TRACE

int t_lvl_get();
void t_lvl_set(int lvl);
void t_lvl_mark(int lvl, char* mark);
void t_lvl_add(int n);
void t_lvl_inc();
void t_lvl_dec();
/* void t_lvl_push(); */
/* void t_lvl_pop(); */

long t_tags_get();
void t_tags_set(long tags);
void t_tag_mark(long tag, char* mark);
void t_tags_on(long tags);
void t_tags_off(long tags);
void t_tag_on(long tags);
void t_tag_off(long tags);
/* void t_tags_push(); */
/* void t_tags_pop(); */

FILE *t_out_get();
void t_out_set(FILE *f);
/* void t_out_push(); */
/* void t_out_pop(); */

int indent(long tags, int n);

/* Output can have multiple (or no [T_NONE]) tags.  Goes to the buffer of the
 * highest tag. */
void _trace(long tags, int lvl, const char *fmt, ...)
  __attribute__ ((format (printf, 3, 4)));

#define error(ts, fmt, args...)   trace((ts), T_ERROR, (fmt), ## args)

#else

#define t_lvl_get(...) 0
#define t_lvl_set(...)
#define t_lvl_add(...)
#define t_lvl_mark(...)
#define t_lvl_inc(...)
#define t_lvl_dec(...)
#define t_lvl_push(...)
#define t_lvl_pop(...)

#define t_tags_get(...) T_NONE
#define t_tags_set(...)
#define t_tag_mark(...)
#define t_tags_on(...)
#define t_tags_off(...)
#define t_tag_on(...)
#define t_tag_off(...)
#define t_tags_push(...)
#define t_tags_pop(...)

#define t_out_get(...) NULL
#define t_out_set(...)
#define t_out_push(...)
#define t_out_pop(...)

#define indent(...)
#define _trace(...)

/* Make `error` and `fatal` available no matter what. */
#define error(_ts, fmt, args...)                 \
  do {                                           \
    fprintf(stderr, "[%s] ", T_ERROR_MARKER);    \
    fprintf(stderr, (fmt), ## args);             \
  } while (0)

#endif

#define t_trace(lvl) ((lvl) <= t_lvl_get())
#define t_error t_trace(T_ERROR)
#define t_warn  t_trace(T_WARN)
#define t_info  t_trace(T_INFO)
#define t_guru  t_trace(T_GURU)
#define t_zen   t_trace(T_ZEN)

#define fatal(fmt, args...)                     \
  do {                                          \
    error(T_NONE, (fmt), ## args);              \
    exit(EXIT_FAILURE);                         \
  } while (0)

/* Wrap all tracing calls in `if`'s as an optimization in case some of the
 * arguments require computations. */
#define trace(ts, lvl, fmt, args...)            \
  do {                                          \
    if (t_trace((lvl))) {                       \
      _trace((ts), (lvl), (fmt), ## args);      \
    }                                           \
  } while (0)
#define warn(ts, fmt, args...)                  \
  do {                                          \
    if (t_warn) {                               \
      _trace((ts), T_WARN, (fmt), ## args);     \
    }                                           \
  } while (0)
#define info(ts, fmt, args...)                  \
  do {                                          \
    if (t_info) {                               \
      _trace((ts), T_INFO, (fmt), ## args);     \
    }                                           \
  } while (0)
#define guru(ts, fmt, args...)                  \
  do {                                          \
    if (t_guru) {                               \
      _trace((ts), T_GURU, (fmt), ## args);     \
    }                                           \
  } while (0)
#define zen(ts, fmt, args...)                   \
  do {                                          \
    if (t_zen) {                                \
      _trace((ts), T_ZEN, (fmt), ## args);      \
    }                                           \
  } while (0)

#endif /* __TRACE_H */
