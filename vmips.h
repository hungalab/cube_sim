/* Definitions to support the main driver program.  -*- C++ -*-
    Original work Copyright 2001, 2003 Brian R. Gaeke.
    Original work Copyright 2002, 2003 Paul Twohey.
    Modified work Copyright (c) 2021 Amano laboratory, Keio University.
        Modifier: Takuya Kojima

    This file is part of CubeSim, a cycle accurate simulator for 3-D stacked system.
    It is derived from a source code of VMIPS project under GPLv2.

    CubeSim is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    CubeSim is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CubeSim.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef _VMIPS_H_
#define _VMIPS_H_

#include "types.h"
#include "deviceexc.h"
#include <cstdio>
#include <new>
#include <vector>

class Mapper;
class CPU;
class IntCtrl;
class Options;
class MemoryModule;
class ROMModule;
class Debug;
class Clock;
class ClockDevice;
class HaltDevice;
class SpimConsoleDevice;
class TerminalController;
class DECRTCDevice;
class DECCSRDevice;
class DECStatDevice;
class DECSerialDevice;
class TestDev;
class Disassembler;
class Interactor;
class RouterInterface;
class RouterRange;
class RouterIOReg;
class CubeAccelerator;
class DMAC;
class AcceleratorDebugger;
class BusConAccelerator;

long timediff(struct timeval *after, struct timeval *before);

class vmips
{
public:
	// Machine states
	enum { HALT, RUN, DEBUG, INTERACT }; 

	Mapper		*physmem;
	CPU		*cpu;
	IntCtrl		*intc;
	Options		*opt;
	MemoryModule	*memmod;
	MemoryModule	*mem_prog;
	ROMModule *rm;
	Debug	*dbgr;
	Disassembler	*disasm;
	bool		host_bigendian;

	int			state;
	bool halted() const { return (state == HALT); }

	Clock		*clock;
	ClockDevice	*clock_device;
	HaltDevice	*halt_device;
	SpimConsoleDevice	*spim_console;
	DECRTCDevice	*decrtc_device;
	DECCSRDevice	*deccsr_device;
	DECStatDevice	*decstat_device;
	DECSerialDevice	*decserial_device;
	TestDev		*test_device;
	RouterInterface *rtif;
	RouterIOReg *rtIO;
	RouterRange *rtrange_kseg0, *rtrange_kseg1;
	CubeAccelerator *ac0, *ac1, *ac2;
	BusConAccelerator *bus_ac0, *bus_ac1, *bus_ac2;
	AcceleratorDebugger *ac0_dbg, *ac1_dbg, *ac2_dbg;

	DMAC *dmac;

	/* Cached versions of options: */
	bool		opt_bootmsg;
	bool		opt_clockdevice;
	bool		opt_debug;
	bool		opt_dumpcpu;
	bool		opt_dumpcp0;
	bool		opt_haltdevice;
	bool		opt_haltdumpcpu;
	bool		opt_haltdumpcp0;
	bool		opt_instcounts;
	bool		opt_memdump;
	bool		opt_realtime;
	bool		opt_decrtc;
	bool		opt_deccsr;
	bool		opt_decstat;
	bool		opt_decserial;
	bool		opt_spimconsole;
	bool		opt_testdev;
	bool		opt_cache_prof;
	bool		opt_exmem_prof;
	bool		opt_router_prof;
	uint32		opt_clockspeed;
	uint32		clock_nanos;
	uint32		opt_clockintr;
	uint32		opt_clockdeviceirq;
	uint32		opt_loadaddr;
	uint32		opt_bootaddr;
	uint32		opt_debuggeraddr;
	uint32		opt_memsize;
	uint32		opt_progmemsize;
	uint32		opt_timeratio;
	char		*opt_image;
	char		*opt_boot;
	char		*opt_execname;
	char		*opt_memdumpfile;
	char		*opt_ttydev;
	char		*opt_ttydev2;
	uint32		num_cycles;
	uint32 		stall_count;
	uint32		mem_bandwidth;
	uint32		bus_latency;
	uint32		exmem_latency;
	uint32		vcbufsize;
	bool		mode_cpu_only;
	bool		mode_cube;
	bool		mode_bus_conn;

private:
	Interactor *interactor;

	/* If boot messages are enabled with opt_bootmsg, print MSG as a
	   printf(3) style format string for the remaing arguments. */
	virtual void boot_msg( const char *msg, ... );

	/* Initialize the SPIM-compatible console device and connect it to
	   configured terminal lines. */
	virtual bool setup_spimconsole();

	/* Initialize the test-only device. */
	virtual bool setup_testdev();

	/* Initialize the clock device if it is configured. Return true if
	   there are no initialization problems, otherwise return false. */
	virtual bool setup_clockdevice();

	/* Initialize the DEC RTC if it is configured. Return true if
	   there are no initialization problems, otherwise return false. */
	virtual bool setup_decrtc();

	/* Initialize the DEC CSR if it is configured. Return true if
	   there are no initialization problems, otherwise return false. */
	virtual bool setup_deccsr();

	/* Initialize the DEC status registers if configured. Return true if
	   there are no initialization problems, otherwise return false. */
	virtual bool setup_decstat();

	/* Initialize the DEC serial device if it is configured. Return true if
	   there are no initialization problems, otherwise return false. */
	virtual bool setup_decserial();

	virtual bool setup_prog();

	virtual bool setup_bootrom();

  	virtual bool setup_rs232c();

	virtual bool setup_exe();

	virtual bool setup_router();

	virtual bool setup_cube();

	virtual bool setup_dmac();

	virtual bool setup_bus_master();

	bool load_elf (FILE *fp);
	bool load_ecoff (FILE *fp);
	char *translate_to_host_ram_pointer (uint32 vaddr);

	virtual bool setup_ram();

	virtual bool setup_clock();

	/* Connect the file or device named NAME to line number L of
	   console device C, or do nothing if NAME is "off".  */
	virtual void setup_console_line(int l, char *name,
		TerminalController *c, const char *c_name);

	/* Initialize the halt device if it is configured. */
	bool setup_haltdevice();

	void check_mode();

	//Bus masters
	std::vector<DeviceExc*> bus_masters;
	int master_count;
	int master_start;

public:
	void refresh_options(void);
	vmips(int argc, char **argv);

	/* Cleanup after we are done. */
	virtual ~vmips();
	
	void setup_machine(void);

	/* Attention key was pressed. */
	void attn_key(void);

	/* Halt the simulation. */
	void halt(void);

	/* Interact with user. */
	bool interact(void);

	int host_endian_selftest(void);

	void dump_cpu_info(bool dumpcpu, bool dumpcp0);

	// function pointer
	typedef void (vmips::*FuncPtr)(void);
	FuncPtr step_ptr;
	void step(void);

	//step func for each mode
	void step_cube(void);
	void step_cpu_only(void);
	void step_bus_conn(void);

	int run(void);
};

extern vmips *machine;

#endif /* _VMIPS_H_ */
