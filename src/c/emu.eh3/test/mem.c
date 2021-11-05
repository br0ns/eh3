#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <stdbool.h>
#include <signal.h>

#include "../mem.h"

extern mmap_t *mem;

#include <assert.h>

void show_maps(void) {
  mmap_t *m = mem;

  fprintf(stderr, "<<< MEMORY MAP >>>\n");
  while (m) {
    if (m->prev) {
      assert(m->prev->next == m);
    }

    fprintf(stderr, "0x%08x--0x%08x %s%s%s (%#x)\n",
            m->addr, m->addr + m->size,
            m->prot & PROT_READ  ? "r" : "-",
            m->prot & PROT_WRITE ? "w" : "-",
            m->prot & PROT_EXEC  ? "x" : "-",
            m->size);
    m = m->next;
  }
}

word do_mmap(word addr, word size) {
  word ret;
  ret = mem_map(addr, size, 7, NULL);
  fprintf(stderr, "mmap(%p, %#x) -> %p\n", addr, size, ret);
  show_maps();
  fprintf(stderr, "\n");
  return ret;
}

word do_find(word addr, word numb) {
  mmap_t *m;
  m = mem_find(addr, numb);
  fprintf(stderr, "find(%p, %#x) -> ", addr, numb);
  if (m) {
    fprintf(stderr, "%p--%p\n\n", m->addr, m->addr + m->size);
    return m->addr;
  } else {
    fprintf(stderr, "NULL\n\n");
    return 0;
  }
}

void do_unmap(word addr, word size) {
  mem_unmap(addr, size);
  fprintf(stderr, "unmap(%p, %#x)\n", addr, size);
  show_maps();
  fprintf(stderr, "\n");
}

void fuzz(void) {
  byte pagemap[1 << (32 - 12)] = {0}, prot;
  word addr, size, lo, hi, i, j, ret;
  bool can_map;
  mmap_t *m;

  srand(time(NULL));

#define randint(min, max) \
  ((min) + ((word)rand() * ((word)RAND_MAX + 1) + (word)rand()) % ((max) - (min) + 1))

#define min(a, b) ((a) <= (b) ? (a) : (b))

#define randvars()                                          \
  do {                                                      \
    addr = randint(0x1000, 0xffffffff);                     \
    size = randint(1, min(0x100000, 0x100000000 - addr));   \
    prot = randint(1, 7);                                   \
    if (addr + size - 1 < addr) {                           \
      printf("addr + size > 0x100000000\n");                \
      exit(1);                                              \
    }                                                       \
    hi = (addr + size - 1) >> 12;                           \
    lo = addr >> 12;                                        \
  } while (0)

#define check()                                         \
  do {                                                  \
    m = mem;                                            \
    i = 0;                                              \
    for (;;) {                                          \
      while (!pagemap[i] && i < sizeof(pagemap)) i++;   \
      if (i == sizeof(pagemap)) break;                  \
                                                        \
      j = i + 1;                                        \
      prot = pagemap[i];                                \
      while (pagemap[j] == prot && j < sizeof(pagemap)) j++;    \
                                                        \
      assert(m->addr >> 12 == i);                       \
      assert(m->size >> 12 == j - i);                   \
      assert(m->prot == prot);                          \
                                                        \
      i = j;                                            \
      m = m->next;                                      \
    }                                                   \
    assert(m == NULL);                                  \
  } while (0)

  for (;;) {
    randvars();
    ret = mem_map(addr, size, prot, NULL);

    can_map = true;
    for (i = lo; i <= hi; i++) {
      if (pagemap[i]) {
        can_map = false;
        break;
      }
    }

    if (!can_map) {
      printf("can't map %#x, %#x\n", addr, size);
      assert(ret == 0);
      continue;
    }

    printf("%#x %#x %x %x\n", addr, size, lo, hi);
    assert(ret == (addr & ~0xfff));
    for (i = lo; i <= hi; i++) {
      pagemap[i] = prot;
    }

    check();
    /* show_maps(); */

    randvars();
    mem_unmap(addr, size);
    for (i = lo; i <= hi; i++) {
      pagemap[i] = 0;
    }

    check();

  }
}

void handler(int signum) {
  show_maps();
  mem_unmap(0x1000, 0x100000000 - 0x1000);
  show_maps();
  assert(!mem);
  exit(EXIT_SUCCESS);
}

int main(void) {
  signal(SIGINT, handler);
  fuzz();
  return 0;

  /* Run two times, to test if things work both cold and hot. */
  for (int i = 0; i < 2; i++) {
    assert(0x1000 == do_mmap(0, 1));
    assert(0x1000 == do_find(0x1000, 1));
    assert(0x1000 == do_find(0x1000, 0x1000));
    assert(0x1000 == do_find(0x1001, 0xfff));
    assert(0x1000 == do_find(0x1fff, 1));
    assert(0x1000 == do_find(0x2000, 0));
    assert(0 == do_find(0x2000, 1));
    assert(0 == do_find(0x1001, 0x1000));
    assert(0 == do_find(0x1000, ~0));
    assert(0 == do_find(~0, 1));
    assert(0 == do_find(~0, ~0));

    assert(0x2000 == do_mmap(0, 1));
    assert(do_mmap(0x4000, 1));

    assert(do_mmap(0x3000, 1));
    assert(0xfffff000 == do_mmap(0xfffff000, 1));
    assert(0xffffe000 == do_mmap(0xffffe000, 1));
    assert(0          == do_mmap(0xffffe000, 1));

    do_unmap(0, 0x1000);
    do_unmap(0x1000, 0x1000);
    assert(0x2000 == mem->addr);
    do_unmap(0x3000, 1);
    assert(0x4000 == mem->next->addr);
    assert(0 == do_mmap(0x3000, 0x2000));
    assert(do_mmap(0x3000, 0x1000));

    do_unmap(0xfffff000, 0x1001);
    do_unmap(0xfffff000, 0x1000);
    assert(do_mmap(0xffffc000, 0x2000));
    assert(do_mmap(0xfffff000, 0x1000));
    do_unmap(0xffffc000, 0x4000);
    do_unmap(0x2000, 0x3000);

    /* do_mmap(0x1000, 0x1000); */
    /* do_mmap(0x2000, 0x1000); */
    /* do_mmap(0x3000, 0x1000); */
    /* do_mmap(0x4000, 0x1000); */
    /* do_mmap(0x5000, 0x1000); */
    /* do_unmap(0x3000, 0x1000); */
    /* do_unmap(0x2000, 0x3000); */
  }
}
