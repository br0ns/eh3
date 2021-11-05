
#ifndef __DEBUG_H
#define __DEBUG_H

#include <stdbool.h>

#include "types.h"
#include "mem.h"
#include "debug.h"

void debug_init(char *exe_path, char* cmds_path);
bool debug_hook();
void debug_fini();

#endif /* __DEBUG_H */
