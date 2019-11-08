# Main project directory
top_builddir = ..

# commands & flags
#	C compile
CC = gcc
DEFS = -DHAVE_CONFIG_H
DEFAULT_INCLUDES = -I. -I$(top_builddir)
INCLUDES =
CPPFLAGS =
CFLAGS = -g -O2
COMPILE = $(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(CPPFLAGS) $(CFLAGS)
#	Lib archive
RANLIB = ranlib

libopcodes_mips_a_SOURCES = \
	dis-buf.c libbfd-support.c mips-dis.c mips-opc.c mips16-opc.c \
	opintl.h sysdep.h config.h bfd.h elf-bfd.h libbfd.h ansidecl.h \
	bfdlink.h dis-asm.h symcat.h

OBJEXT = o

libopcodes_mips_a_LIBADD =
libopcodes_mips_a_OBJECTS = dis-buf.$(OBJEXT) \
	libbfd-support.$(OBJEXT) mips-dis.$(OBJEXT) mips-opc.$(OBJEXT) \
	mips16-opc.$(OBJEXT)

.SUFFIXES:
.SUFFIXES: .c .o .obj
.PHONY: all clean

all: libopcodes_mips.a

%.o: %.c
	$(COMPILE) -c -o $@ $<

%.obj: %.c
	$(COMPILE) -c -o $@ $<

# dependencies
libopcodes_mips.a: $(libopcodes_mips_a_OBJECTS)
	-rm -f libopcodes_mips.a
	$(AR) $(ARFLAGS) libopcodes_mips.a $(libopcodes_mips_a_OBJECTS) $(libopcodes_mips_a_LIBADD)
	$(RANLIB) libopcodes_mips.a

dis-buf.o: dis-buf.c sysdep.h config.h ansidecl.h \
  dis-asm.h \
  bfd.h symcat.h \
  opintl.h

libbfd-support.o: libbfd-support.c \
  bfd.h ansidecl.h symcat.h libbfd.h

mips-dis.o: mips-dis.c sysdep.h config.h ansidecl.h \
  dis-asm.h \
  bfd.h symcat.h opcode/mips.h opintl.h elf-bfd.h elf/common.h \
  elf/internal.h elf/external.h bfdlink.h elf/mips.h elf/reloc-macros.h

mips-opc.o: mips-opc.c \
  sysdep.h config.h ansidecl.h \
  opcode/mips.h

mips16-opc.o: mips16-opc.c \
  sysdep.h config.h ansidecl.h \
  opcode/mips.h

clean:
	-rm -f *.o
