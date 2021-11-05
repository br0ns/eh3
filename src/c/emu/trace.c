#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* Make lint tool see the right header file. */
#ifndef TRACE
#define TRACE
#endif
#include "trace.h"

/* T_DEFAULT_OUT (stderr) is not a compile time constant. */
static FILE *_t_out;
#define t_out (_t_out ? _t_out : T_DEFAULT_OUT)

static int t_lvl = T_DEFAULT_LVL;
static struct {
  bool on;
  char *mark;
} t_tags[T_TAGS];
char *t_lvls[T_LEVELS] = {
  NULL,
  T_ERROR_MARKER,
  T_WARN_MARKER,
  T_INFO_MARKER,
  T_GURU_MARKER,
  T_ZEN_MARKER,
};
static int t_ind[T_TAGS + 1];

/* Line buffering.  Index `T_TAGS` for un-tagged output. */
static char* t_bufs[T_TAGS + 1][T_LEVELS];

int t_lvl_get() {
  return t_lvl;
}

void t_lvl_set(int lvl) {
  if (lvl >= 0 && lvl < T_LEVELS) {
    t_lvl = lvl;
  }
}

void t_lvl_mark(int lvl, char *mark) {
  if (lvl >= 0 && lvl < T_LEVELS) {
    t_lvls[lvl] = mark;
  }
}

void t_lvl_add(int n) {
  t_lvl_set(t_lvl + n);
}

void t_lvl_inc() {
  if (t_lvl + 1 < T_LEVELS) {
    t_lvl++;
  }
}

void t_lvl_dec() {
  if (t_lvl - 1 >= 0) {
    t_lvl--;
  }
}

long t_tags_get() {
  long tags = 0;
  unsigned i;
  for (i = 0; i < T_TAGS; i++) {
    if (t_tags[i].on) {
      tags |= 1 << i;
    }
  }
  return tags;
}

void t_tags_set(long tags) {
  unsigned i;
  for (i = 0; i < T_TAGS; i++) {
    t_tags[i].on = tags & (1 << i);
  }
}

void t_tag_mark(long tag, char *mark) {
  unsigned i;
  for (i = 0; i < T_TAGS; i++) {
    if (tag & (1 << i)) {
      t_tags[i].mark = mark;
      return;
    }
  }
}

void t_tags_on(long tags) {
  unsigned i;
  for (i = 0; i < T_TAGS; i++) {
    if (tags & (1 << i)) {
      t_tags[i].on = true;
    }
  }
}

void t_tags_off(long tags) {
  unsigned i;
  for (i = 0; i < T_TAGS; i++) {
    if (tags & (1 << i)) {
      t_tags[i].on = false;
    }
  }
}

FILE *t_out_get() {
  return t_out;
}

void t_out_set(FILE *out) {
  _t_out = out;
}

/* Get effective tag. */
static long t_tag(long tags) {
  unsigned tag = 0, i;

  if (!tags) {
    return T_TAGS;
  }
  for (i = 0; i < T_TAGS; i++) {
    if (tags & (1 << i)) {
      tag = i;
      /* break; */
    }
  }
  return tag;
}

void t_tag_on(long tags) {
  t_tags[t_tag(tags)].on = true;
}

void t_tag_off(long tags) {
  t_tags[t_tag(tags)].on = false;
}

int indent(long tags, int n) {
  long tag;

  tag = t_tag(tags);

  t_ind[tag] += n;
  if (t_ind[tag] < 0) {
    t_ind[tag] = 0;
  }

  return t_ind[tag];
}

#define t_puts(s)              fputs((s), t_out)
#define t_printf(fmt, args...) fprintf(t_out, (fmt), ## args)
#define t_write(buf, len)      fwrite((buf), 1, (len), t_out)

void _trace(long tags, int lvl, const char *fmt, ...) {
  char *s, *nl, *buf, *sep;
  unsigned i;
  int ret, tag;
  bool on;
  va_list args;

  if (t_lvl >= T_LEVELS) {
    t_lvl = T_LEVELS - 1;
  }

  /* Un-tagged output is always enabled. */
  if (T_NONE != tags) {
    on = false;
    for (i = 0; i < T_TAGS; i++) {
      if (tags & (1 << i)) {
        on |= t_tags[i].on;
      }
    }
    if (!on) {
      return;
    }
  }

  va_start(args, fmt);

  tag = t_tag(tags);
  buf = t_bufs[tag][lvl];

  if (-1 == (ret = vasprintf(&s, fmt, args))) {
    perror("vasprintf");
    exit(EXIT_FAILURE);
  }

  if (buf) {
    buf = realloc(buf, strlen(buf) + ret + 1);
    if (!buf) {
      perror("realloc");
      exit(EXIT_FAILURE);
    }
    memcpy(buf + strlen(buf), s, ret + 1);
    free(s);
  } else {
    buf = s;
  }
  t_bufs[tag][lvl] = buf;

  while ((nl = strchr(buf, '\n'))) {

    sep = "";
    t_puts("[");
    if (t_lvls[lvl]) {
      t_puts(sep);
      sep = ":";
      t_puts(t_lvls[lvl]);
    }
    for (i = 0; i < T_TAGS; i++) {
      if (tags & (1 << i) && t_tags[i].mark) {
        t_puts(sep);
        sep = ".";
        t_puts(t_tags[i].mark);
      }
    }
    t_puts("] ");
    t_printf("%*s", t_ind[tag], "");
    t_write(buf, nl - buf + 1);
    buf = nl + 1;
  }

  if (*buf) {
    buf = strdup(buf);
  } else {
    buf = NULL;
  }

  free(t_bufs[tag][lvl]);
  t_bufs[tag][lvl] = buf;

  va_end(args);
}
