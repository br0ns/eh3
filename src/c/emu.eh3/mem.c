#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "util.h"
#include "consts.h"
#include "mem.h"

/* #define PAGE_TABLE */

/* XXX: May contain all kinds of overflow errors! */

#define ROUND_SIZE(size)                        \
  do {                                          \
    size = (size + PAGE_SIZE - 1) & ~PAGE_MASK; \
  } while (0)

#define ROUND_ADDR_SIZE(addr, size)                                  \
  do {                                                               \
    size = ((addr & PAGE_MASK) + size + PAGE_SIZE - 1) & ~PAGE_MASK; \
    addr &= ~PAGE_MASK;                                              \
  } while (0)

/* Use this to avoid overflow/underflow when we know that `size` and `addr` are
 * not both 0. */
#define LAST_BYTE(addr, size) ((addr) + (size) - 1)
#define LAST_BYTE_M(m) LAST_BYTE((m)->addr, (m)->size)

mmap_t *mem = NULL;

#ifdef PAGE_TABLE

mmap_t *ptbl[MEM_SIZE / PAGE_SIZE];

static void update_page_table() {
  mmap_t *m = mem;
  word i, j;

  memset(ptbl, 0, ((m ? m->addr : MEM_SIZE) / PAGE_SIZE) * sizeof(*ptbl));
  while (m) {
    j = (m->next ? m->next->addr : MEM_SIZE) / PAGE_SIZE;
    for (i = m->addr / PAGE_SIZE; i < j; i++) {
      ptbl[i] = m;
    }
    m = m->next;
  }
}

/* Return highest mapping starting no higher than `addr`. */
static inline
mmap_t *mem_find_(word addr) {
  return ptbl[addr / PAGE_SIZE];
}

#else

#define update_page_table()

/* Return highest mapping starting no higher than `addr`. */
static inline
mmap_t *mem_find_(word addr) {
  mmap_t *m = mem;

  /* No mappings, or `addr` is below first mapping. */
  if (!m || m->addr > addr) {
    return NULL;
  }

  /* Keep going until last mapping or next mapping is above `addr`. */
  while (m->next && m->next->addr <= addr) {
    m = m->next;
  }

  return m;
}

#endif

/* Return mapping containing range. */
inline
mmap_t *mem_find(word addr, word numb) {
  mmap_t *m = mem_find_(addr);

  if (m && m->addr + m->size - numb >= addr) {
    return m;
  } else {
    return NULL;
  }
}

static inline
word mmap_new(mmap_t *prev, word addr, word size, byte prot, mmap_t **mem_out) {
  word prev_end, this_end;
  mmap_t *this, *next;

  this_end = addr + size;

  if (prev) {
    next = prev->next;
    prev_end = prev->addr + prev->size;
  } else {
    next = mem;
    prev_end = MMAP_MIN_ADDR;
  }

  /* Disallow empty mappings. */
  if (0 == size) {
    return 0;
  }

  /* Does the mapping exceed memory? */
  if (this_end - 1 < addr) {
    return 0;
  }

  /* Does the new mapping fit? */
  /* XXX: ncc outputs a signed comparison here because of the `- 1`.  This error
   *      is probably everywhere. */
  if (addr <= (prev_end - 1) ||
      (next && next->addr <= (this_end - 1))) {
    return 0;
  }

  /* Merge with previous? */
  if (prev && addr == prev_end && prot == prev->prot) {
    this = prev;
    this->size += size;
    this->data = do_realloc(this->data, this->size);
  } else {
    this = do_malloc(sizeof(mmap_t));
    this->addr = addr;
    this->size = size;
    this->prot = prot;
    this->data = do_malloc(size);
    this->prev = prev;
    this->next = next;

    /* Insert. */
    if (next) {
      next->prev = this;
    }
    if (prev) {
      prev->next = this;
    } else {
      mem = this;
    }
  }

  /* Merge with next? */
  if (next && this_end == next->addr && prot == next->prot) {
    this->size += next->size;
    this->data = do_realloc(this->data, this->size);
    memcpy(&this->data[this->size - next->size],
           next->data, next->size);
    this->next = next->next;
    if (this->next) {
      this->next->prev = this;
    }
    free(next->data);
    free(next);
  }

  if (mem_out) {
    *mem_out = this;
  }

  update_page_table();

  return addr;
}

/* Create a new mapping at a random address. */
static inline
word mmap_random(word size, byte prot, mmap_t **mem_out) {
  mmap_t *prev;

  ROUND_SIZE(size);

  /* No mappings yet, or enough space before first one. */
  if (!mem || MMAP_MIN_ADDR + size < mem->addr) {
    return mmap_new(NULL, MMAP_MIN_ADDR, size, prot, mem_out);
  }

  /* Else place after first mapping with a gap large enough between it and the
   * next, or lacking that, the last one. */
  else {
    prev = mem;
    while (prev->next &&
           prev->next->addr - (prev->addr + prev->size) < size) {
      prev = prev->next;
    }
    /* Place just after this mapping. */
    return mmap_new(prev, prev->addr + prev->size, size, prot, mem_out);
  }
}

/* Create a new mapping at a fixed address. */
static inline
word mmap_fixed(word addr, word size, byte prot, mmap_t **mem_out) {
  mmap_t *prev;

  ROUND_ADDR_SIZE(addr, size);
  prev = mem_find_(addr);

  return mmap_new(prev, addr, size, prot, mem_out);
}

/* Create a new mapping.  If `addr` is NULL, choose a random address. */
word mem_map(word addr, word size, byte prot, mmap_t **mem_out) {
  if (0 == addr) {
    return mmap_random(size, prot, mem_out);
  } else {
    return mmap_fixed(addr, size, prot, mem_out);
  }
}

/* Un-map all mapped memory in range. */
void mem_unmap(word addr, word size) {
  mmap_t *lo, *hi, *tmp;
  word trim;

  ROUND_ADDR_SIZE(addr, size);

  /* Empty range. */
  if (!size) {
    return;
  }

  /* Range extend past end of memory. */
  if (LAST_BYTE(addr, size) < addr) {
    size = -addr;
  }

  /* Un-map all mappings completely covered by range.  `lo` and `hi` are the
   * mappings closest to the range, but not completely covered by it.  They may
   * or may not exist or touch the range.  Those cases are handled below. */
  lo = mem_find_(addr);
  if (lo) {
    if (lo->addr == addr) {
      hi = lo;
      lo = lo->prev;
    } else {
      hi = lo->next;
    }
  } else {
    hi = mem;
  }

  /* hi: |-------------|
       ----------------|
       OR
       ------------------|
   */
  while (hi && LAST_BYTE_M(hi) <= LAST_BYTE(addr, size)) {
    /* Un-link. */
    if (hi->prev) {
      hi->prev->next = hi->next;
    } else {
      mem = hi->next;
    }
    if (hi->next) {
      hi->next->prev = hi->prev;
    }

    /* Go to next. */
    tmp = hi;
    hi = hi->next;

    /* Free. */
    free(tmp->data);
    free(tmp);
  }

  /* lo:  |------------|
                  |--
  */
  if (lo && LAST_BYTE_M(lo) >= addr) {
    /* lo: |------------|
              |------|
     */
    if (LAST_BYTE_M(lo) > LAST_BYTE(addr, size)) {
      /* Create new mapping for tail end of `lo`. */
      tmp = do_malloc(sizeof(mmap_t));
      tmp->addr = addr + size;
      tmp->size = lo->addr + lo->size - tmp->addr;
      tmp->prot = lo->prot;
      tmp->data = do_malloc(tmp->size);
      tmp->next = lo->next;
      if (lo->next) {
        lo->next->prev = tmp;
      }
      tmp->prev = lo;
      lo->next = tmp;
      memcpy(tmp->data, &lo->data[tmp->addr - lo->addr], tmp->size);
    }

    /* Trim from start. */
    trim = lo->addr + lo->size - addr;
    lo->size -= trim;
    lo->data = do_realloc(lo->data, lo->size);
  }

  /* hi:  |------------|
          --|
  */
  if (hi && LAST_BYTE(addr, size) >= hi->addr) {
    trim = addr + size - hi->addr;
    hi->size -= trim;
    hi->addr += trim;
    memmove(hi->data, &hi->data[trim], hi->size);
    hi->data = do_realloc(hi->data, hi->size);
  }

  update_page_table();

}
