import sys
import struct

from consts import *
from obj import *

ELFCLASS32    = 1
ELFDATA2LSB   = 1
ELFOSABI_SYSV = 0
EV_CURRENT    = 1
ET_REL        = 1
EM_EH3        = 12
SHT_PROGBITS  = 1
SHT_SYMTAB    = 2
SHT_STRTAB    = 3
SHT_NOBITS    = 8
SHT_REL       = 9
SHF_WRITE     = 1<<0
SHF_ALLOC     = 1<<1
SHF_EXECINSTR = 1<<2
SHF_INFO_LINK = 1<<6
SHN_UNDEF     = 0
STB_LOCAL     = 0
STB_GLOBAL    = 1
STB_WEAK      = 2
STT_NOTYPE    = 0
STV_DEFAULT   = 0

def p8(n):
    return struct.pack('B', n)

def p16(n):
    return struct.pack('<H', n)

def p32(n):
    return struct.pack('<I', n)

strtab = '\0' # empty string for SHN_UNDEF
def newstr(s):
    global strtab
    s += '\0'
    o = strtab.find(s)
    if o >= 0:
        return o
    o = len(strtab)
    strtab += s
    return o

def getstr(o):
    return strtab[o:].split('\0', 1)[0]

symtab = ['\0' * 0x10] # STN_UNDEF
def newsym(name, value, size, bind, shndx):
    st_name = newstr(name)
    st_value = value
    st_size = size
    st_bind = {
        BIND_GLOBAL: STB_GLOBAL,
        BIND_WEAK: STB_WEAK,
        BIND_LOCAL: STB_LOCAL,
    }[bind]
    st_type = STT_NOTYPE
    st_info = st_bind << 4 | st_type
    st_other = STV_DEFAULT
    st_shndx = shndx

    symtab.append(
        p32(st_name) +
        p32(st_value) +
        p32(st_size) +
        p8(st_info) +
        p8(st_other) +
        p16(st_shndx)
    )

    return len(symtab) - 1

def getsym(name):
    for i, s in enumerate(symtab):
        st_name, = struct.unpack('<I', s[0:4])
        if name == getstr(st_name):
            return i
    # Symbol not found, add undefined symbol
    return newsym(name, 0, 0, BIND_GLOBAL, SHN_UNDEF)

def outelf(fd, o):
    offset = 52 # Ehdr size

    # Section headers
    shdrs = [
        '\0' * 40, # SHN_UNDEF
        None,      # symtab
        None,      # strtab
    ]
    SHN_SYMTAB = 1
    SHN_STRTAB = 2
    SHN_OFFSET = len(shdrs)

    # Add defined symbols
    for symbol in o.symbols:
        newsym(
            symbol.name,
            symbol.offset,
            symbol.size,
            symbol.bind,
            symbol.section + SHN_OFFSET
        )

    # Add sections
    for i, section in enumerate(o.sections):
        if section.data and len(section.data) != section.size:
            print >>sys.stderr, 'Padding section "%s" with zeros.' % \
                section.name
            section.data += [0] * (section.size - len(section.data))

        sh_name = newstr(section.name)
        if section.data:
            sh_type = SHT_PROGBITS
        else:
            sh_type = SHT_NOBITS
        sh_flags = SHF_ALLOC
        if section.name == '.text':
            sh_flags |= SHF_EXECINSTR
        else:
            sh_flags |= SHF_WRITE
        sh_addr = 0
        sh_offset = offset
        sh_size = section.size
        if sh_type == SHT_PROGBITS:
            offset += sh_size
        sh_link = 0
        sh_info = 0
        sh_addralign = 1
        sh_entsize = 0

        shdrs.append(
            p32(sh_name) +
            p32(sh_type) +
            p32(sh_flags) +
            p32(sh_addr) +
            p32(sh_offset) +
            p32(sh_size) +
            p32(sh_link) +
            p32(sh_info) +
            p32(sh_addralign) +
            p32(sh_entsize)
        )

        # Collect relocations related to this section
        reltab = []
        for reloc in o.relocations:
            if i != reloc.section:
                continue
            if reloc.size != 32:
                print >>sys.stderr, 'Only 32bit relocations supported'
                sys.exit(1)
            r_offset = reloc.offset
            r_sym = getsym(reloc.symbol)
            r_type = reloc.type + 1
            r_info = r_sym << 8 | r_type
            reltab.append(
                p32(r_offset) +
                p32(r_info)
            )
        section.reltab = reltab

    # Add relocation sections
    for i, section in enumerate(o.sections):
        if not section.reltab:
            continue
        sh_name = newstr('.rel' + section.name)
        sh_type = SHT_REL
        sh_flags = SHF_INFO_LINK
        sh_addr = 0
        sh_offset = offset
        sh_size = len(section.reltab) * 8
        offset += sh_size
        sh_link = SHN_SYMTAB
        sh_info = i + SHN_OFFSET
        sh_addralign = 1
        sh_entsize = 8

        shdrs.append(
            p32(sh_name) +
            p32(sh_type) +
            p32(sh_flags) +
            p32(sh_addr) +
            p32(sh_offset) +
            p32(sh_size) +
            p32(sh_link) +
            p32(sh_info) +
            p32(sh_addralign) +
            p32(sh_entsize)
        )

    # Add symtab and strtab sections
    sh_name = newstr('.symtab')
    sh_type = SHT_SYMTAB
    sh_flags = 0
    sh_addr = 0
    sh_offset = offset
    sh_size = len(symtab) * 16
    offset += sh_size
    sh_link = SHN_STRTAB
    sh_info = 0
    sh_addralign = 4
    sh_entsize = 16
    shdrs[SHN_SYMTAB] = \
        p32(sh_name) + \
        p32(sh_type) + \
        p32(sh_flags) + \
        p32(sh_addr) + \
        p32(sh_offset) + \
        p32(sh_size) + \
        p32(sh_link) + \
        p32(sh_info) + \
        p32(sh_addralign) + \
        p32(sh_entsize)

    sh_name = newstr('.strtab')
    sh_type = SHT_STRTAB
    sh_flags = 0
    sh_addr = 0
    sh_offset = offset
    sh_size = len(strtab)
    offset += sh_size
    sh_link = 0
    sh_info = 0
    sh_addralign = 1
    sh_entsize = 0
    shdrs[SHN_STRTAB] = \
        p32(sh_name) + \
        p32(sh_type) + \
        p32(sh_flags) + \
        p32(sh_addr) + \
        p32(sh_offset) + \
        p32(sh_size) + \
        p32(sh_link) + \
        p32(sh_info) + \
        p32(sh_addralign) + \
        p32(sh_entsize)

    # Write ELF file
    ei_class = ELFCLASS32
    ei_data = ELFDATA2LSB
    ei_version = EV_CURRENT
    ei_osabi = ELFOSABI_SYSV
    ei_abiversion = 0
    ei_pad = '       '
    e_ident = \
        '\x7fELF' + \
        p8(ei_class) + \
        p8(ei_data) + \
        p8(ei_version) + \
        p8(ei_osabi) + \
        p8(ei_abiversion) + \
        ei_pad
    e_type = ET_REL
    e_machine = EM_EH3
    e_version = 1
    e_entry = 0
    e_phoff = 0
    e_shoff = offset
    e_flags = 0
    e_ehsize = 52
    e_phentsize = 0
    e_phnum = 0
    e_shentsize = 40
    e_shnum = len(shdrs)
    e_shstrndx = SHN_STRTAB
    fd.write(e_ident)
    fd.write(p16(e_type))
    fd.write(p16(e_machine))
    fd.write(p32(e_version))
    fd.write(p32(e_entry))
    fd.write(p32(e_phoff))
    fd.write(p32(e_shoff))
    fd.write(p32(e_flags))
    fd.write(p16(e_ehsize))
    fd.write(p16(e_phentsize))
    fd.write(p16(e_phnum))
    fd.write(p16(e_shentsize))
    fd.write(p16(e_shnum))
    fd.write(p16(e_shstrndx))

    # Write section data and relocation tables
    for section in o.sections:
        fd.write(str(section.data))
    for section in o.sections:
        fd.write(''.join(section.reltab))

    # Write symtab
    fd.write(''.join(symtab))

    # Write strtab
    fd.write(strtab)

    # Write shdrs
    fd.write(''.join(shdrs))
