#ifndef __UTIL_H
#define __UTIL_H

#include <unistd.h>

void *do_malloc(size_t size);
void *do_calloc(size_t nmemb, size_t size);
void *do_realloc(void *ptr, size_t size);
ssize_t do_read(int fd, void *buf, size_t count);
void readn(int fd, void *buf, size_t count);

#endif /* __UTIL_H */
