/* Main driver program for VMIPS.
   Copyright 2001, 2003 Brian R. Gaeke.
   Copyright 2002, 2003 Paul Twohey.

This file is part of VMIPS.

VMIPS is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.

VMIPS is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with VMIPS; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

#include "clock.h"
#include "clockdev.h"
#include "clockreg.h"
#include "cpzeroreg.h"
#include "debug.h"
#include "error.h"
#include "endiantest.h"
#include "haltreg.h"
#include "haltdev.h"
#include "intctrl.h"
#include "range.h"
#include "spimconsole.h"
#include "mapper.h"
#include "memorymodule.h"
#include "cpu.h"
#include "cpzero.h"
#include "spimconsreg.h"
#include "vmips.h"
#include "options.h"
#include "decrtc.h"
#include "decrtcreg.h"
#include "deccsr.h"
#include "deccsrreg.h"
#include "decstat.h"
#include "decserial.h"
#include "testdev.h"
#include "stub-dis.h"
#include "rommodule.h"
#include "interactor.h"
#include <fcntl.h>
#include <cerrno>
#include <csignal>
#include <cstdarg>
#include <cstring>
#include <string>
#include <exception>
#include "rs232c.h"
#include "routerinterface.h"
#include "accelerator.h"
#include "remoteram.h"
#include "cma.h"
#include "snacc.h"
#include "dmac.h"

vmips *machine;

void
vmips::refresh_options(void)
{
	/* Extract important flags and things. */
	opt_bootmsg = opt->option("bootmsg")->flag;
	opt_clockdevice = opt->option("clockdevice")->flag;
	opt_debug = opt->option("debug")->flag;
	opt_dumpcpu = opt->option("dumpcpu")->flag;
	opt_dumpcp0 = opt->option("dumpcp0")->flag;
	opt_haltdevice = opt->option("haltdevice")->flag;
	opt_haltdumpcpu = opt->option("haltdumpcpu")->flag;
	opt_haltdumpcp0 = opt->option("haltdumpcp0")->flag;
	opt_instcounts = opt->option("instcounts")->flag;
	opt_memdump = opt->option("memdump")->flag;
	opt_realtime = opt->option("realtime")->flag;
	opt_cache_prof = opt->option("cacheprof")->flag;
 
	opt_clockspeed = opt->option("clockspeed")->num;
	clock_nanos = 1000000000/opt_clockspeed;

	opt_clockintr = opt->option("clockintr")->num;
	opt_clockdeviceirq = opt->option("clockdeviceirq")->num;
	opt_loadaddr = opt->option("loadaddr")->num;
	opt_bootaddr = opt->option("bootaddr")->num;
	opt_memsize = opt->option("memsize")->num;
	opt_progmemsize = opt->option("progmemsize")->num;
	opt_timeratio = opt->option("timeratio")->num;
 
	opt_memdumpfile = opt->option("memdumpfile")->str;
	opt_image = opt->option("romfile")->str;
	opt_boot = opt->option("bootfile")->str;
	opt_execname = opt->option("execname")->str;
	opt_ttydev = opt->option("ttydev")->str;
	opt_ttydev2 = opt->option("ttydev2")->str;

	opt_decrtc = opt->option("decrtc")->flag;
	opt_deccsr = opt->option("deccsr")->flag;
	opt_decstat = opt->option("decstat")->flag;
	opt_decserial = opt->option("decserial")->flag;
	opt_spimconsole = opt->option("spimconsole")->flag;
	opt_testdev = opt->option("testdev")->flag;
	mem_bandwidth = opt->option("mem_bandwidth")->num;
	mem_access_latency = opt->option("mem_access_latency")->num;
	vcbufsize = opt->option("vcbufsize")->num;


}

/* Set up some machine globals, and process command line arguments,
 * configuration files, etc.
 */
vmips::vmips(int argc, char *argv[])
	: opt(new Options), state(HALT),
	  clock(0), clock_device(0), halt_device(0), spim_console(0),
	  num_cycles(0), interactor(0), stall_count(0)
{
    opt->process_options (argc, argv);
	refresh_options();
}

vmips::~vmips()
{
	if (disasm) delete disasm;
	if (opt_debug && dbgr) delete dbgr;
	if (cpu) delete cpu;
	if (physmem) delete physmem;
	//if (clock) delete clock;  // crash in this dtor - double free?
	if (intc) delete intc;
	if (opt) delete opt;
	if (rtif) delete rtif;
	if (ac0) delete ac0;
	if (ac1) delete ac1;
	if (ac2) delete ac2;
	if (dmac) delete dmac;
}

void
vmips::setup_machine(void)
{
	/* Construct the various vmips components. */
	intc = new IntCtrl;
	physmem = new Mapper;
	cpu = new CPU (*physmem, *intc);

	/* Set up the debugger interface, if applicable. */
	if (opt_debug)
		dbgr = new Debug (*cpu, *physmem);

    /* Direct the libopcodes disassembler output to stderr. */
    disasm = new Disassembler (host_bigendian, stderr);
}

/* Connect the file or device named NAME to line number L of
 * console device C, or do nothing if NAME is "off".
 * If NAME is "stdout", then the device will be connected to stdout.
 */
void vmips::setup_console_line(int l, char *name, TerminalController *c, const
char *c_name)
{
	/* If they said to turn off the tty line, do nothing. */
	if (strcmp(name, "off") == 0)
		return;

	int ttyfd;
	if (strcmp(name, "stdout") == 0) {
		/* If they asked for stdout, give them stdout. */
		ttyfd = fileno(stdout);
	} else {
		/* Open the file or device in question. */
		ttyfd = open(name, O_RDWR | O_NONBLOCK);
		if (ttyfd == -1) {
			/* If we can't open it, warn and use stdout instead. */
			error("Opening %s (terminal %d): %s", name, l,
				strerror(errno));
			warning("using stdout, input disabled\n");
			ttyfd = fileno(stdout);
		}
	}

	/* Connect it to the SPIM-compatible console device. */
	c->connect_terminal(ttyfd, l);
	boot_msg("Connected fd %d to %s line %d.\n", ttyfd, c_name, l);
}

bool vmips::setup_spimconsole()
{
	/* FIXME: It would be helpful to restore tty modes on a SIGINT or
	   other abortive exit or when vmips has been foregrounded after
	   being in the background. The restoration mechanism should use
	   TerminalController::reinitialze_terminals() */

	if (!opt_spimconsole)
		return true;
	
	spim_console = new SpimConsoleDevice( clock );
	physmem->map_at_physical_address( spim_console, SPIM_BASE );
	boot_msg("Mapping %s to physical address 0x%08x\n",
		  spim_console->descriptor_str(), SPIM_BASE);
	
	intc->connectLine(IRQ2, spim_console);
	intc->connectLine(IRQ3, spim_console);
	intc->connectLine(IRQ4, spim_console);
	intc->connectLine(IRQ5, spim_console);
	intc->connectLine(IRQ6, spim_console);
	boot_msg("Connected IRQ2-IRQ6 to %s\n",spim_console->descriptor_str());

	setup_console_line(0, opt_ttydev, spim_console,
		spim_console->descriptor_str ());
	setup_console_line(1, opt_ttydev2, spim_console,
		spim_console->descriptor_str ());
	return true;
}

bool vmips::setup_clockdevice()
{
	if( !opt_clockdevice )
		return true;

	uint32 clock_irq;
	if( !(clock_irq = DeviceInt::num2irq( opt_clockdeviceirq )) ) {
		error( "invalid clockdeviceirq (%u), irq numbers must be 2-7.",
		       opt_clockdeviceirq );
		return false;
	}	

	/* Microsecond Clock at base physaddr CLOCK_BASE */
	clock_device = new ClockDevice( clock, clock_irq, opt_clockintr );
	physmem->map_at_physical_address( clock_device, CLOCK_BASE );
	boot_msg( "Mapping %s to physical address 0x%08x\n",
		  clock_device->descriptor_str(), CLOCK_BASE );

	intc->connectLine( clock_irq, clock_device );
	boot_msg( "Connected %s to the %s\n", DeviceInt::strlineno(clock_irq),
		  clock_device->descriptor_str() );

	return true;
}

bool vmips::setup_decrtc()
{
	if (!opt_decrtc)
		return true;

	/* Always use IRQ3 ("hw interrupt level 1") for RTC. */
	uint32 decrtc_irq = DeviceInt::num2irq (3);

	/* DECstation 5000/200 DS1287-based RTC at base physaddr DECRTC_BASE */
	decrtc_device = new DECRTCDevice( clock, decrtc_irq );
	physmem->map_at_physical_address( decrtc_device, DECRTC_BASE );
	boot_msg( "Mapping %s to physical address 0x%08x\n",
		  decrtc_device->descriptor_str(), DECRTC_BASE );

	intc->connectLine( decrtc_irq, decrtc_device );
	boot_msg( "Connected %s to the %s\n", DeviceInt::strlineno(decrtc_irq),
		  decrtc_device->descriptor_str() );

	return true;
}

bool vmips::setup_deccsr()
{
	if (!opt_deccsr)
		return true;

	/* DECstation 5000/200 Control/Status Reg at base physaddr DECCSR_BASE */
    /* Connected to IRQ2 */
    static const uint32 DECCSR_MIPS_IRQ = DeviceInt::num2irq (2);
	deccsr_device = new DECCSRDevice (DECCSR_MIPS_IRQ);
	physmem->map_at_physical_address (deccsr_device, DECCSR_BASE);
	boot_msg ("Mapping %s to physical address 0x%08x\n",
		  deccsr_device->descriptor_str(), DECCSR_BASE);

	intc->connectLine (DECCSR_MIPS_IRQ, deccsr_device);
    boot_msg("Connected %s to the %s\n", DeviceInt::strlineno(DECCSR_MIPS_IRQ),
             deccsr_device->descriptor_str());

	return true;
}

bool vmips::setup_decstat()
{
	if (!opt_decstat)
		return true;

	/* DECstation 5000/200 CHKSYN + ERRADR at base physaddr DECSTAT_BASE */
	decstat_device = new DECStatDevice( );
	physmem->map_at_physical_address( decstat_device, DECSTAT_BASE );
	boot_msg( "Mapping %s to physical address 0x%08x\n",
		  decstat_device->descriptor_str(), DECSTAT_BASE );

	return true;
}

bool vmips::setup_decserial()
{
	if (!opt_decserial)
		return true;

	/* DECstation 5000/200 DZ11 serial at base physaddr DECSERIAL_BASE */
	/* Uses CSR interrupt SystemInterfaceCSRInt */
	decserial_device = new DECSerialDevice (clock, SystemInterfaceCSRInt);
	physmem->map_at_physical_address (decserial_device, DECSERIAL_BASE );
	boot_msg ("Mapping %s to physical address 0x%08x\n",
		  decserial_device->descriptor_str (), DECSERIAL_BASE );

	// Use printer line for console.
	setup_console_line (3, opt_ttydev, decserial_device,
      decserial_device->descriptor_str ());
	return true;
}

bool vmips::setup_testdev()
{
	if (!opt_testdev)
		return true;

	test_device = new TestDev();
	physmem->map_at_physical_address(test_device, TEST_BASE);
	boot_msg("Mapping %s to physical address 0x%08x\n",
		 test_device->descriptor_str(), TEST_BASE);
	return true;
}

bool vmips::setup_haltdevice()
{
	if( !opt_haltdevice )
		return true;

	halt_device = new HaltDevice( this );
	physmem->map_at_physical_address( halt_device, HALT_BASE );
	boot_msg( "Mapping %s to physical address 0x%08x\n",
		  halt_device->descriptor_str(), HALT_BASE );

	return true;
}

void vmips::boot_msg( const char *msg, ... )
{
	if( !opt_bootmsg )
		return;

	va_list ap;
	va_start( ap, msg );
	vfprintf( stderr, msg, ap );
	va_end( ap );

	fflush( stderr );
}

int
vmips::host_endian_selftest(void)
{
  try {
    EndianSelfTester est;
    machine->host_bigendian = est.host_is_big_endian();
    if (!machine->host_bigendian) {
      boot_msg ("Little-Endian host processor detected.\n");
    } else {
      boot_msg ("Big-Endian host processor detected.\n");
    }
    return 0;
  } catch (std::string &err) {
    boot_msg (err.c_str ());
    return -1;
  }
}

void
vmips::halt(void)
{
	state = HALT;
}

void
vmips::attn_key(void)
{
    state = INTERACT;
}

void vmips::dump_cpu_info(bool dumpcpu, bool dumpcp0) {
	if (dumpcpu) {
		cpu->dump_regs (stderr);
		cpu->dump_stack (stderr);
	}
	if (dumpcp0)
		cpu->cpzero_dump_regs_and_tlb (stderr);
}

void
vmips::step(void)
{
	/* Process instructions. */
	cpu->step();
	if (dmac != NULL) dmac->step();
	rtif->step();

	if (ac0 != NULL) ac0->step();
	if (ac1 != NULL) ac1->step();
	if (ac2 != NULL) ac2->step();

	/* Keep track of time passing. Each instruction either takes
	 * clock_nanos nanoseconds, or we use pass_realtime() to check the
	 * system clock.
     */
	if( !opt_realtime )
	   clock->increment_time(clock_nanos);
	else
	   clock->pass_realtime(opt_timeratio);

	/* If user requested it, dump registers from CPU and/or CP0. */
    dump_cpu_info (opt_dumpcpu, opt_dumpcp0);

	num_cycles++;
}

long 
timediff(struct timeval *after, struct timeval *before)
{
    return (after->tv_sec * 1000000 + after->tv_usec) -
        (before->tv_sec * 1000000 + before->tv_usec);
}

bool
vmips::setup_prog ()
{
  // Open prog image.
  FILE *bin_file = fopen (opt_image, "rb");
  if (!bin_file) {
    error ("Could not open program binary `%s': %s", opt_image, strerror (errno));
    return false;
  }
  // Translate loadaddr to physical address.

  try {
    mem_prog = new MemoryModule(opt_progmemsize, bin_file);
  } catch (int errcode) {
    error ("mmap failed for %s: %s", opt_image, strerror (errcode));
    return false;
  }
  // Map the prog image to the virtual physical memory.
  physmem->map_at_physical_address (mem_prog, opt_loadaddr);

  boot_msg ("Mapping program binary (%s, %u words) to physical address 0x%08x\n",
            opt_image, mem_prog->getExtent () / 4, mem_prog->getBase ());

  return true;
}

bool
vmips::setup_bootrom ()
{
  // Open BootROM image.
  FILE *rom = fopen (opt_boot, "rb");
  if (!rom) {
    error ("Could not open Boot ROM `%s': %s", opt_boot, strerror (errno));
    return false;
  }
  // Translate loadaddr to physical address.
  ROMModule *rm;
  try {
    rm = new ROMModule (rom);
  } catch (int errcode) {
    error ("mmap failed for %s: %s", opt_boot, strerror (errcode));
    return false;
  }
  // Map the ROM image to the virtual physical memory.
  physmem->map_at_physical_address (rm, opt_bootaddr);

  boot_msg ("Mapping Boot ROM image (%s, %u words) to physical address 0x%08x\n",
            opt_boot, rm->getExtent () / 4, rm->getBase ());
  // Point debugger at wherever the user thinks the ROM is.
  if (opt_debug)
    if (dbgr->setup (opt_bootaddr, rm->getExtent () / 4) < 0)
      return false; // Error in setting up debugger.
  return true;
}

bool
vmips::setup_ram ()
{
  // Make a new RAM module and install it at base physical address 0.
  memmod = new MemoryModule(opt_memsize);
  physmem->map_at_physical_address(memmod, 0);

  // memmod2 = new MemoryModule(0x100000);
  // physmem->map_at_physical_address(memmod2, 0x80000000);

  boot_msg( "Mapping RAM module (host=%p, %uKB) to physical address 0x%x\n",
	    memmod->getAddress (), memmod->getExtent () / 1024, memmod->getBase ());
  return true;
}

bool
vmips::setup_clock ()
{
  /* Set up the clock with the current time. */
  timeval start;
  gettimeofday(&start, NULL);
  timespec start_ts;
  TIMEVAL_TO_TIMESPEC( &start, &start_ts );
  clock = new Clock( start_ts );
  return true;
}

bool
vmips::setup_rs232c()
{
	Rs232c *rs232c = new Rs232c();
	if (physmem->map_at_physical_address(rs232c, 0xb4000000) == 0) {
		boot_msg("Succeeded in setup rs232c serial IO\n");
		return true;
	} else {
		boot_msg("Failed in setup rs232c serial IO\n");
		return false;
	}
}

bool
vmips::setup_router()
{
	rtif = new RouterInterface();
	rtIO = new RouterIOReg(rtif);
	rtrange_kseg0 = new RouterRange(rtif, false);
	rtrange_kseg1 = new RouterRange(rtif, true);

	//make instance
	if (physmem->map_at_physical_address(rtIO, 0xba010000) == 0) {
		if (physmem->map_at_physical_address(rtrange_kseg0, 0xba400000) == 0) {
			if (physmem->map_at_physical_address(rtrange_kseg1, 0x9a400000) == 0) {
				boot_msg("Succeeded in setup cube router\n");
			} else {
				boot_msg("Failed in setup router range (kseg1)\n");
				return false;
			}
		} else {
			boot_msg("Failed in setup router range (kseg0)\n");
			return false;
		}
	} else {
		boot_msg("Failed in setup router IO reg\n");
		return false;
	}

	//connect IRQ line
	intc->connectLine(IRQ5, rtif);
	boot_msg( "Connected IRQ5 to the %s\n", rtif->descriptor_str());
	return true;

}

bool vmips::setup_dmac()
{
	if (opt->option("dmac")->flag) {
		dmac = new DMAC(*physmem);
	}
	return true;
}

bool
vmips::setup_cube()
{
	std::string ac0_name = std::string(opt->option("accelerator0")->str);
	std::string ac1_name = std::string(opt->option("accelerator1")->str);
	std::string ac2_name = std::string(opt->option("accelerator2")->str);

	//setup accelerator0
	if (ac0_name == std::string("CMA")) {
		ac0 = new CMA(1, rtif->getRouter());
	} else if (ac0_name == std::string("SNACC")) {
		ac0 = new SNACC(1, rtif->getRouter(), 4);
	} else if (ac0_name == std::string("RemoteRam")) {
		ac0 = new RemoteRam(1, rtif->getRouter(), 0x2048); //2KB
	} else if (ac0_name != std::string("none")) {
		fprintf(stderr, "Unknown accelerator: %s\n", ac0_name.c_str());
		return false;
	}

	if (ac0 != NULL) {
		ac0->setup();
	}

	//setup accelerator1
	if (ac1_name != std::string("none")) {
		if (ac0 == NULL){
			fprintf(stderr, "Upper module (accelerator0) is not built\n");
			return false;
		} if (ac1_name == std::string("CMA")) {
			ac1 = new CMA(2, ac0->getRouter());
		} else if (ac1_name == std::string("SNACC")) {
			ac1 = new SNACC(2, ac0->getRouter(), 4);
		} else if (ac1_name == std::string("RemoteRam")) {
			ac1 = new RemoteRam(2, ac0->getRouter(), 0x2048); //2KB
		} else {
			fprintf(stderr, "Unknown accelerator: %s\n", ac1_name.c_str());
			return false;
		}
	}

	if (ac1 != NULL) {
		ac1->setup();
	}

	//setup accelerator2
	if (ac2_name != std::string("none")) {
		if (ac1 == NULL){
			fprintf(stderr, "Upper module (accelerator1) is not built\n");
			return false;
		} if (ac1_name == std::string("CMA")) {
			ac2 = new CMA(3, ac1->getRouter());
		} else if (ac1_name == std::string("SNACC")) {
			ac2 = new SNACC(3, ac1->getRouter(), 4);
		} else if (ac1_name == std::string("RemoteRam")) {
			ac2 = new RemoteRam(3, ac1->getRouter(), 0x2048); //2KB
		} else {
			fprintf(stderr, "Unknown accelerator: %s\n", ac1_name.c_str());
			return false;
		}
	}

	if (ac2 != NULL) {
		ac2->setup();
	}

	return true;

}

static void
halt_machine_by_signal (int sig)
{
  machine->halt();
}

/// Interact with user. Returns true if we should continue, false otherwise.
///
bool
vmips::interact ()
{
  TerminalController *c;
  if (opt_spimconsole) c = spim_console;
  else if (opt_decserial) c = decserial_device;
  else c = 0;
  if (c) c->suspend();
  bool should_continue = true;
  printf ("\n");
  if (!interactor) interactor = create_interactor ();
  interactor->interact ();
  if (state == INTERACT) state = RUN;
  if (state == HALT) should_continue = false;
  if (c) c->reinitialize_terminals ();
  return should_continue;
}

int
vmips::run()
{
	/* Check host processor endianness. */
	if (host_endian_selftest () != 0) {
		error( "Could not determine host processor endianness." );
		return 1;
	}

	/* Set up the rest of the machine components. */
	setup_machine();

	if (!setup_bootrom ()) 
	  return 1;

	if (!setup_prog ()) 
	  return 1;

	if (!setup_ram ())
	  return 1;

	if (!setup_haltdevice ())
	  return 1;

	if (!setup_clock ())
	  return 1;

	if (!setup_clockdevice ())
	  return 1;

	if (!setup_decrtc ())
	  return 1;

	if (!setup_deccsr ())
	  return 1;

	if (!setup_decstat ())
	  return 1;

	if (!setup_decserial ())
	  return 1;

	// if (!setup_spimconsole ())
	//   return 1;

	// if (!setup_testdev ())
	//   return 1;

	if (!setup_rs232c())
	  return 1;

	if (!setup_router())
	  return 1;

	if (!setup_cube())
	  return 1;

	if (!setup_dmac())
		return 1;

	signal (SIGQUIT, halt_machine_by_signal);

	boot_msg( "Hit Ctrl-\\ to halt machine, Ctrl-_ for a debug prompt.\n" );

	/* Reset the CPU. */
	boot_msg( "\n*************RESET*************\n" );
	boot_msg("Resetting CPU\n");
	cpu->reset();
	/* Reset Router interface */
	if (rtif != NULL) {
		boot_msg("Resetting Router Interface\n");
		rtif->reset();
	}
	if (ac0 != NULL) {
		boot_msg("Resetting %s_0\n", ac0->accelerator_name());
		ac0->reset();
	}
	if (ac1 != NULL) {
		boot_msg("Resetting %s_1\n", ac1->accelerator_name());
		ac1->reset();
	}
	if (ac2 != NULL) {
		boot_msg("Resetting %s_2\n", ac2->accelerator_name());
		ac2->reset();
	}

	if (dmac != NULL) {
		boot_msg("Resetting DMAC\n");
		dmac->reset();
	}

	fprintf(stderr, "\n");

	if (!setup_exe ())
	  return 1;

	timeval start;
	if (opt_instcounts)
		gettimeofday(&start, NULL);

	state = (opt_debug ? DEBUG : RUN);
	while (state != HALT) {
		switch (state) {
			case RUN:
			    	while (state == RUN) { step (); }
				break;
			case DEBUG:
				while (state == DEBUG) { dbgr->serverloop(); }
				break;
			case INTERACT:
				while (state == INTERACT) { interact(); }
				break;
		}
	}

	timeval end;
	if (opt_instcounts)
		gettimeofday(&end, NULL);

	/* Halt! */
	boot_msg( "\n*************HALT*************\n\n" );

	/* If we're tracing, dump the trace. */
	cpu->flush_trace ();

	/* If user requested it, dump registers from CPU and/or CP0. */
	if (opt_haltdumpcpu || opt_haltdumpcp0) {
		fprintf(stderr,"Dumping:\n");
		dump_cpu_info (opt_haltdumpcpu, opt_haltdumpcp0);
	}

	if (opt_instcounts) {
		double elapsed = (double) timediff(&end, &start) / 1000000.0;
		fprintf(stderr, "%u cycles in %.5f seconds (%.3f "
			"instructions per second) (stall ratio %.3f%%)\n", num_cycles, elapsed,
			((double) (num_cycles - stall_count)) / elapsed, ((double) stall_count / (double)num_cycles) * 100);
	}

	if (opt_memdump) {
		fprintf(stderr,"Dumping RAM to %s...", opt_memdumpfile);
		if (FILE *ramdump = fopen (opt_memdumpfile, "wb")) {
			fwrite (memmod->getAddress (), memmod->getExtent (), 1, ramdump);
			fclose(ramdump);
			fprintf(stderr,"succeeded.\n");
		} else {
			error( "\nRAM dump failed: %s", strerror(errno) );
		}
	}

	if (opt_cache_prof) {
		fprintf(stderr, "Instruction Cache Profile\n");
		cpu->icache->report_prof();
		fprintf(stderr, "Data Cache Profile\n");
		cpu->dcache->report_prof();
	}

	/* We're done. */
	boot_msg( "Goodbye.\n" );
	return 0;
}

static void vmips_unexpected() {
  fatal_error ("unexpected exception");
}

static void vmips_terminate() {
  fatal_error ("uncaught exception");
}

int
main(int argc, char **argv)
try {
	std::set_unexpected(vmips_unexpected);
	std::set_terminate(vmips_terminate);

	machine = new vmips(argc, argv);
	int rc = machine->run();
	delete machine; /* No disassemble Number Five!! */
	return rc;
}
catch( std::bad_alloc &b ) {
	fatal_error( "unable to allocate memory" );
}

