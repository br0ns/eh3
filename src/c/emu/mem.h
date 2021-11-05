#ifndef __MEM_H
#define __MEM_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/mman.h>

#include "consts.h"
#include "types.h"

typedef struct _mmap_t {
  word addr, size;
  byte *data, prot;
  /* CONSIDER: Move these to internal repr?  Expose through function calls? */
  struct _mmap_t *prev, *next;
} mmap_t;
extern mmap_t *mem;

/* Checks if a mapping contains a memory range. */
#define mem_contains(m, a, n)                               \
  ((m)->addr <= (a) && (m)->addr + (m)->size - (n) >= (a))  \

/* Return mapping containing range. */
mmap_t *mem_find(word addr, word numb);

/* Create a new mapping.  If `addr` is NULL, choose a random address.  If
 * `mem_out` is non-NULL and memory is successfully mapped, returns the mapping
 * which the requested range lies in. */
word mem_map(word addr, word size, byte prot, mmap_t **mem_out);

/* Un-map all mapped memory in range. */
void mem_unmap(word addr, word size);

/* Change protection flags of memory in range. */
void mem_protect(word addr, word size, byte prot);

#endif /* __MEM_H */
