# Config
SYSCONFDIR = "."
DEFS = -DHAVE_CONFIG_H
DEFAULT_INCLUDES = -I.
INCLUDES =
EXEEXT =
PACKAGE = cube_sim
MAKE = make

# commands & flags
#		CPP compile
CXX = g++
CPPFLAGS =
CXXFLAGS = -g -O2 -std=c++11 -fno-strict-aliasing -I./libopcodes_mips -DSYSCONFDIR=\"$(SYSCONFDIR)\"
CXXCOMPILE = $(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(CPPFLAGS) $(CXXFLAGS)
#		Linker
LDFLAGS =
CXXLD = $(CXX)
CXXLINK = $(CXXLD) $(CXXFLAGS) $(LDFLAGS) -o $@

OBJEXT = o

SOURCES = cpu.cc cpzero.cc devicemap.cc \
  mapper.cc options.cc range.cc intctrl.cc \
  spimconsole.cc stub-dis.cc vmips.cc \
  deviceint.cc debug.cc remotegdb.cc clockdev.cc error.cc \
  clock.cc terminalcontroller.cc haltdev.cc decrtc.cc deccsr.cc \
  decstat.cc decserial.cc rommodule.cc fileutils.cc exeloader.cc fpu.cc \
  interactor.cc testdev.cc \
  rs232c.cc cache.cc busarbiter.cc \
  cpu.h cpzero.h cpzeroreg.h deviceint.h \
  devicemap.h intctrl.h mapper.h memorymodule.h options.h optiontbl.h \
  range.h spimconsole.h spimconsreg.h \
  tlbentry.h vmips.h debug.h remotegdb.h \
  accesstypes.h deviceexc.h clockdev.h excnames.h error.h \
  clock.h task.h terminalcontroller.h haltdev.h devreg.h clockreg.h \
  haltreg.h wipe.h stub-dis.h decrtc.h decrtcreg.h deccsr.h deccsrreg.h \
  decstat.h decserial.h decserialreg.h rommodule.h gccattr.h mmapglue.h \
  types.h endiantest.h fileutils.h fpu.h interactor.h testdev.h \
  rs232c.h cache.h busarbiter.h

OBJECTS = cpu.$(OBJEXT) cpzero.$(OBJEXT) devicemap.$(OBJEXT) \
	mapper.$(OBJEXT) options.$(OBJEXT) range.$(OBJEXT) \
	intctrl.$(OBJEXT) spimconsole.$(OBJEXT) stub-dis.$(OBJEXT) \
	vmips.$(OBJEXT) deviceint.$(OBJEXT) debug.$(OBJEXT) \
	remotegdb.$(OBJEXT) clockdev.$(OBJEXT) error.$(OBJEXT) \
	clock.$(OBJEXT) terminalcontroller.$(OBJEXT) haltdev.$(OBJEXT) \
	decrtc.$(OBJEXT) deccsr.$(OBJEXT) decstat.$(OBJEXT) \
	decserial.$(OBJEXT) rommodule.$(OBJEXT) fileutils.$(OBJEXT) \
	exeloader.$(OBJEXT) fpu.$(OBJEXT) interactor.$(OBJEXT) \
	testdev.$(OBJEXT) rs232c.$(OBJEXT) cache.$(OBJEXT) busarbiter.$(OBJEXT)

LDADD = libopcodes_mips/libopcodes_mips.a

DEPENDENCIES = libopcodes_mips/libopcodes_mips.a

$(PACKAGE)$(EXEEXT): $(OBJECTS) $(DEPENDENCIES)
	@rm -f $(PACKAGE)$(EXEEXT)
	$(CXXLINK) $(OBJECTS) $(LDADD) $(LIBS)

%.o: %.cc
	$(CXXCOMPILE) -c -o $@ $<

%.obj: %.cc
	$(CXXCOMPILE) -c -o $@ $<

libopcodes_mips/libopcodes_mips.a:
	cd libopcodes_mips && $(MAKE) all

.SUFFIXES:
.SUFFIXES: .cc .o .obj

.PHONY: all test clean

all: $(PACKAGE)$(EXEEXT)

test:
	cd test_vec && $(MAKE) all

clean:
	cd libopcodes_mips && $(MAKE) clean
	-rm -f *.o

cpu.o: cpu.cc cpu.h deviceexc.h accesstypes.h types.h config.h state.h \
  vmips.h mapper.h range.h \
  cpzero.h tlbentry.h cpzeroreg.h debug.h \
  options.h \
  excnames.h error.h gccattr.h remotegdb.h fileutils.h stub-dis.h \
  libopcodes_mips/bfd.h libopcodes_mips/ansidecl.h \
  libopcodes_mips/symcat.h libopcodes_mips/dis-asm.h ISA.h

cpzero.o: cpzero.cc cpzero.h tlbentry.h config.h cpzeroreg.h types.h \
  mapper.h range.h accesstypes.h \
  excnames.h cpu.h deviceexc.h state.h \
  vmips.h intctrl.h error.h gccattr.h options.h

devicemap.o: devicemap.cc accesstypes.h range.h types.h config.h \
  devicemap.h cpu.h deviceexc.h state.h \
  vmips.h mapper.h

mapper.o: mapper.cc cpu.h deviceexc.h accesstypes.h types.h config.h \
  vmips.h mapper.h range.h \
  devicemap.h error.h gccattr.h excnames.h memorymodule.h rommodule.h \
  options.h busarbiter.h

options.o: options.cc error.h gccattr.h config.h fileutils.h \
  types.h options.h \
  optiontbl.h

range.o: range.cc range.h accesstypes.h types.h config.h \
  error.h gccattr.h

intctrl.o: intctrl.cc deviceint.h intctrl.h types.h config.h

spimconsole.o: spimconsole.cc clock.h task.h types.h config.h \
  mapper.h range.h accesstypes.h \
  spimconsole.h deviceint.h intctrl.h devicemap.h terminalcontroller.h \
  devreg.h \
  spimconsreg.h vmips.h

stub-dis.o: stub-dis.cc stub-dis.h types.h config.h \
  libopcodes_mips/bfd.h libopcodes_mips/ansidecl.h \
  libopcodes_mips/symcat.h libopcodes_mips/dis-asm.h

vmips.o: vmips.cc clock.h task.h types.h config.h \
  clockdev.h deviceint.h intctrl.h \
  devicemap.h range.h accesstypes.h \
  devreg.h clockreg.h cpzeroreg.h debug.h deviceexc.h state.h \
  error.h gccattr.h endiantest.h \
  haltreg.h haltdev.h spimconsole.h terminalcontroller.h \
  mapper.h memorymodule.h cpu.h \
  vmips.h cpzero.h tlbentry.h spimconsreg.h options.h decrtc.h \
  decrtcreg.h deccsr.h deccsrreg.h decstat.h decserial.h decserialreg.h \
  testdev.h stub-dis.h libopcodes_mips/bfd.h libopcodes_mips/ansidecl.h \
  libopcodes_mips/symcat.h libopcodes_mips/dis-asm.h rommodule.h \
  interactor.h \
  rs232c.h

deviceint.o: deviceint.cc deviceint.h intctrl.h types.h config.h \
  vmips.h

debug.o: debug.cc debug.h deviceexc.h accesstypes.h types.h config.h \
  remotegdb.h cpu.h \
  vmips.h mapper.h range.h \
  excnames.h cpzeroreg.h options.h

remotegdb.o: remotegdb.cc remotegdb.h types.h config.h

clockdev.o: clockdev.cc clockdev.h clock.h task.h types.h config.h \
  deviceint.h intctrl.h \
  devicemap.h range.h accesstypes.h \
  devreg.h

error.o: error.cc gccattr.h config.h

clock.o: clock.cc clock.h task.h types.h config.h \
  error.h gccattr.h wipe.h

terminalcontroller.o: terminalcontroller.cc clock.h task.h types.h \
  error.h gccattr.h terminalcontroller.h devreg.h

haltdev.o: haltdev.cc haltdev.h devicemap.h range.h accesstypes.h types.h \
  config.h

decrtc.o: decrtc.cc devicemap.h range.h accesstypes.h types.h config.h \
  clock.h task.h \
  decrtcreg.h decrtc.h \
  deviceint.h intctrl.h \
  vmips.h

deccsr.o: deccsr.cc deccsrreg.h deccsr.h devicemap.h range.h \
  accesstypes.h types.h config.h \
  deviceint.h intctrl.h

decstat.o: decstat.cc deviceexc.h accesstypes.h types.h config.h state.h \
  vmips.h mapper.h range.h \
  decstat.h devicemap.h

decserial.o: decserial.cc cpu.h deviceexc.h accesstypes.h types.h \
  config.h state.h \
  vmips.h mapper.h range.h \
  deccsr.h devicemap.h deviceint.h intctrl.h decserial.h decserialreg.h \
  terminalcontroller.h devreg.h task.h

rommodule.o: rommodule.cc rommodule.h range.h accesstypes.h types.h \
  config.h \
  fileutils.h mmapglue.h

fileutils.o: fileutils.cc fileutils.h \
  types.h config.h

exeloader.o: exeloader.cc \
  vmips.h types.h config.h cpzeroreg.h memorymodule.h range.h \
  accesstypes.h \
  error.h gccattr.h

fpu.o: fpu.cc fpu.h types.h config.h cpu.h deviceexc.h accesstypes.h \
  vmips.h mapper.h range.h \
  excnames.h stub-dis.h libopcodes_mips/bfd.h libopcodes_mips/ansidecl.h \
  libopcodes_mips/symcat.h libopcodes_mips/dis-asm.h

interactor.o: interactor.cc \
  interactor.h cpu.h deviceexc.h accesstypes.h types.h config.h state.h \
  vmips.h mapper.h range.h

testdev.o: testdev.cc cpu.h deviceexc.h accesstypes.h types.h config.h \
  vmips.h mapper.h range.h \
  testdev.h devicemap.h

rs232c.o: rs232c.cc rs232c.h deviceint.h intctrl.h types.h config.h \
  devicemap.h range.h accesstypes.h \
  mapper.h

cache.o: cache.cc cache.h \
  types.h config.h deviceexc.h accesstypes.h state.h vmips.h \
  mapper.h range.h \
  excnames.h

busarbiter.o: busarbiter.cc busarbiter.h