import re

emu = file('../emu/emu.c').read()
emu = re.sub(r'/\* Preprocessor .*? icache_t;',
             'typedef uint32_t icache_t;\n'
             '#define ICACHE_SIZE 32',
             emu,
             flags=re.DOTALL)

# emu = re.sub(r'#include "trace\.h"\n',
#              '',
#              emu)

# def repl(m):
#     print m.groups()

# emu = re.sub(r'(trace|fatal|error|warn|info|guru|zen)\([A-Z_]*,(.*?)\);',
#              repl,
#              emu,
#              flags=re.DOTALL)

# print emu
file('emu.c', 'w').write(emu)
