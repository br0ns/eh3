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
void trace(long tags, int lvl, const char *fmt, ...);

void warn(long tags, const char *fmt, ...);
void info(long tags, const char *fmt, ...);
void guru(long tags, const char *fmt, ...);
void zen(long tags, const char *fmt, ...);

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
#define trace(...)
#define warn(...)
#define info(...)
#define guru(...)
#define zen(...)

#endif

#define t_trace(lvl) ((lvl) <= t_lvl_get())
#define t_error t_trace(T_ERROR)
#define t_warn  t_trace(T_WARN)
#define t_info  t_trace(T_INFO)
#define t_guru  t_trace(T_GURU)
#define t_zen   t_trace(T_ZEN)

void error(long tags, const char *fmt, ...);
void fatal(const char *fmt, ...);

#endif /* __TRACE_H */
