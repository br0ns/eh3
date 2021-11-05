#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "util.h"

void *do_malloc(size_t size) {
  void *ret;

  ret = malloc(size);
  if (NULL == ret) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }

  return ret;
}

void *do_calloc(size_t nmemb, size_t size) {
  void *ret;

  ret = calloc(nmemb, size);
  if (NULL == ret) {
    perror("calloc");
    exit(EXIT_FAILURE);
  }

  return ret;
}

void *do_realloc(void *ptr, size_t size) {
  void *ret;

  ret = realloc(ptr, size);
  if (NULL == ret && size) {
    perror("realloc");
    exit(EXIT_FAILURE);
  }

  return ret;
}

inline
ssize_t do_read(int fd, void *buf, size_t count) {
  ssize_t res;
  for (;;) {
    res = read(fd, buf, count);
    if (-1 == res && EINTR == errno) {
      continue;
    }
    return res;
  }
}

void readn(int fd, void *buf, size_t count) {
  ssize_t res;
  while (count) {
    res = do_read(fd, buf, count);
    if (-1 == res) {
      perror("read");
      exit(EXIT_FAILURE);
    }
    count -= res;
    buf += res;
  }
}
