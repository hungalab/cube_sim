# Makefile to build CubeSim on x86-64 computers
# Copyright (c) 2021 Amano laboratory, Keio University.
#     Author: Takuya Kojima

# This file is part of CubeSim, a cycle accurate simulator for 3-D stacked system.

# CubeSim is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.

# CubeSim is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with CubeSim.  If not, see <https://www.gnu.org/licenses/>.


# Config
SYSCONFDIR = "."
DEFAULT_INCLUDES = -I.
INCLUDES =
EXEEXT =
PACKAGE = cube_sim
DEFS = -DHAVE_CONFIG_H
MAKE = make

# commands & flags
#		CPP compile
CXX = g++
CPPFLAGS = -ffriend-injection
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
  dmac.h dmac.cc  rs232c.h cache.h busarbiter.h \
  routerinterface.cc routerinterface.h router.cc router.h \
  accelerator.h accelerator.cc  \
  remoteram.h remoteram.cc cma.h cma.cc cmamodules.cc cmamodules.h \
  cmaAddressMap.h dbuf.h dbuf.cc snacc.cc snacc.h snacccore.cc snacccore.h \
  snaccAddressMap.h snaccmodules.cc snaccmodules.h \
  debugutils.cc debugutils.h

OBJECTS = cpu.$(OBJEXT) cpzero.$(OBJEXT) devicemap.$(OBJEXT) \
	mapper.$(OBJEXT) options.$(OBJEXT) range.$(OBJEXT) \
	intctrl.$(OBJEXT) spimconsole.$(OBJEXT) stub-dis.$(OBJEXT) \
	vmips.$(OBJEXT) deviceint.$(OBJEXT) debug.$(OBJEXT) \
	remotegdb.$(OBJEXT) clockdev.$(OBJEXT) error.$(OBJEXT) \
	clock.$(OBJEXT) terminalcontroller.$(OBJEXT) haltdev.$(OBJEXT) \
	decrtc.$(OBJEXT) deccsr.$(OBJEXT) decstat.$(OBJEXT) \
	decserial.$(OBJEXT) rommodule.$(OBJEXT) fileutils.$(OBJEXT) \
	exeloader.$(OBJEXT) fpu.$(OBJEXT) interactor.$(OBJEXT) \
	testdev.$(OBJEXT) rs232c.$(OBJEXT) cache.$(OBJEXT) \
  dmac.$(OBJEXT) busarbiter.$(OBJEXT) \
  routerinterface.${OBJEXT} router.${OBJEXT} \
  remoteram.${OBJEXT} accelerator.${OBJEXT} \
  cma.${OBJEXT} cmamodules.${OBJEXT} dbuf.${OBJEXT} \
  snacc.${OBJEXT} snacccore.${OBJEXT} snaccmodules.${OBJEXT} \
  debugutils.${OBJEXT}

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
  libopcodes_mips/symcat.h libopcodes_mips/dis-asm.h ISA.h cacheinstr.h

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
  interactor.h rs232c.h routerinterface.h remoteram.h accelerator.h \
  cma.h snacc.h dmac.h debugutils.h

deviceint.o: deviceint.cc deviceint.h intctrl.h types.h config.h \
  vmips.h

debug.o: debug.cc debug.h deviceexc.h accesstypes.h types.h config.h \
  remotegdb.h cpu.h \
  vmips.h mapper.h range.h \
  excnames.h cpzeroreg.h options.h debugutils.h

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

interactor.o: interactor.cc interactor.h cpu.h deviceexc.h \
    accesstypes.h types.h config.h state.h \
    vmips.h mapper.h range.h

testdev.o: testdev.cc cpu.h deviceexc.h accesstypes.h \
    types.h config.h vmips.h mapper.h range.h \
    testdev.h devicemap.h

rs232c.o: rs232c.cc rs232c.h deviceint.h intctrl.h types.h \
    config.h devicemap.h range.h accesstypes.h mapper.h

cache.o: cache.cc cache.h \
  types.h config.h deviceexc.h accesstypes.h state.h vmips.h \
  mapper.h range.h \
  excnames.h cacheinstr.h

dmac.o: dmac.cc dmac.h deviceexc.h mapper.h range.h \
          accesstypes.h deviceint.h

busarbiter.o: busarbiter.cc busarbiter.h

routerinterface.o: routerinterface.cc routerinterface.h\
    devicemap.h deviceexc.h router.h accelerator.h deviceint.h \
    accelerator.h excnames.h options.h accesstypes.h

router.o: router.cc router.h vmips.h options.h

accelerator.o: accelerator.h accelerator.cc  \
    range.h router.h error.h options.h vmips.h debugutils.h

remoteram.o: remoteram.cc remoteram.h accelerator.h \
                memorymodule.h debugutils.h

dbuf.o: dbuf.h dbuf.cc range.h types.h fileutils.h vmips.h options.h

cma.o: cma.cc cma.h accelerator.h dbuf.h accesstypes.h\
    types.h cmamodules.h cmaAddressMap.h debugutils.h

cmamodules.o: cmamodules.h cmamodules.cc cmaAddressMap.h range.h \
    dbuf.h accelerator.h 

snacc.o: snacc.h snacc.cc dbuf.h snaccAddressMap.h snaccmodules.h \
        debugutils.h

snacccore.o: snacccore.h snacccore.cc vmips.h options.h \
    snaccmodules.h snaccAddressMap.h

snaccmodules.o: snaccmodules.h snaccmodules.cc snaccAddressMap.h \
    accesstypes.h

debugutils.o: debugutils.cc debugutils.h devicemap.h vmips.h mapper.h
