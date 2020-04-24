#include "snacc.h"
#include "snaccmodules.h"

using namespace SNACCComponents;

SNACC::SNACC(uint32 node_ID, Router* upperRouter, int core_count_)
	: CubeAccelerator(node_ID, upperRouter,
						SNACC_GLB_OUTOFRANGE, false),
	core_count(core_count_)
{
	cores = new SNACCCore*[core_count];
	dmem_upper = new DoubleBuffer*[core_count];
	dmem_lower = new DoubleBuffer*[core_count];
	rbuf_upper = new DoubleBuffer*[core_count];
	rbuf_lower = new DoubleBuffer*[core_count];
	imem = new DoubleBuffer*[core_count];
	lut = new DoubleBuffer*[core_count];
	wbuf = new DoubleBuffer(SNACC_WBUF_SIZE);

	wbuf_arb = new WbufArb(core_count);

	for (int i = 0; i < core_count; i++) {
		dmem_upper[i] = new DoubleBuffer(SNACC_DMEM_SIZE);
		dmem_lower[i] = new DoubleBuffer(SNACC_DMEM_SIZE);
		rbuf_upper[i] = new DoubleBuffer(SNACC_RBUF_SIZE);
		rbuf_lower[i] = new DoubleBuffer(SNACC_RBUF_SIZE);
		imem[i] = new DoubleBuffer(SNACC_IMEM_SIZE);
		lut[i] = new DoubleBuffer(SNACC_LUT_SIZE);
		cores[i] = new SNACCCore(i, dmem_upper[i], dmem_lower[i],
								 rbuf_upper[i], rbuf_lower[i],
								lut[i], imem[i], wbuf, wbuf_arb);
	}

	//broadcast range
	bcast_dmem_lower = new BCastRange(SNACC_DMEM_SIZE, core_count, dmem_lower);
	bcast_dmem_upper = new BCastRange(SNACC_DMEM_SIZE, core_count, dmem_upper);
	bcast_rbuf_lower = new BCastRange(SNACC_RBUF_SIZE, core_count, rbuf_lower);
	bcast_rbuf_upper = new BCastRange(SNACC_RBUF_SIZE, core_count, rbuf_upper);
	bcast_imem = new BCastRange(SNACC_IMEM_SIZE, core_count, imem);
	bcast_lut = new BCastRange(SNACC_LUT_SIZE, core_count, lut);


	confReg = new ConfRegCtrl(core_count, dmem_upper, dmem_lower,
								rbuf_upper, rbuf_lower, imem, lut, wbuf);

	cleared = new bool[core_count] {false} ;

}

SNACC::~SNACC()
{

}

void SNACC::setup()
{
	int core_top_addr;
	for (int i = 0; i < core_count; i++) {
		core_top_addr = SNACC_CORE_ADDR_SIZE * i;
		localBus->map_at_local_address(dmem_upper[i], core_top_addr +
										SNACC_GLB_DMEM_UPPER_OFFSET);
		localBus->map_at_local_address(dmem_lower[i], core_top_addr +
										SNACC_GLB_DMEM_LOWER_OFFSET);
		localBus->map_at_local_address(rbuf_upper[i], core_top_addr +
										SNACC_GLB_RBUF_UPPER_OFFSET);
		localBus->map_at_local_address(rbuf_lower[i], core_top_addr +
										SNACC_GLB_RBUF_LOWER_OFFSET);
		localBus->map_at_local_address(imem[i], core_top_addr +
										SNACC_GLB_IMEM_OFFSET);
		localBus->map_at_local_address(lut[i], core_top_addr +
										SNACC_GLB_LUT_OFFSET);
	}
	localBus->map_at_local_address(wbuf, SNACC_GLB_WBUF_OFFSET);
	localBus->map_at_local_address(confReg, SNACC_GLB_CONF_REG_OFFSET);

	localBus->map_at_local_address(bcast_dmem_lower, SNACC_GLB_BCASE_OFFSET +
									SNACC_GLB_DMEM_LOWER_OFFSET);
	localBus->map_at_local_address(bcast_dmem_upper, SNACC_GLB_BCASE_OFFSET +
									SNACC_GLB_DMEM_UPPER_OFFSET);
	localBus->map_at_local_address(bcast_rbuf_lower, SNACC_GLB_BCASE_OFFSET +
									SNACC_GLB_RBUF_LOWER_OFFSET);
	localBus->map_at_local_address(bcast_rbuf_lower, SNACC_GLB_BCASE_OFFSET +
									SNACC_GLB_RBUF_UPPER_OFFSET);
	localBus->map_at_local_address(bcast_imem, SNACC_GLB_BCASE_OFFSET +
									SNACC_GLB_IMEM_OFFSET);
	localBus->map_at_local_address(bcast_lut, SNACC_GLB_BCASE_OFFSET +
									SNACC_GLB_LUT_OFFSET);

}


void SNACC::core_step()
{
	static bool dbg[4] = {false};
	static bool dbg2[4] = {false};
	static int dbg_counter = 0;
	for (int i = 0; i < core_count; i++) {
		if (confReg->isStart(i)) {
			if (!dbg[i]) {
				fprintf(stderr, "%d run core %d\n", machine->num_cycles, i);
				dbg[i] = true;
			}
			cores[i]->step();
			if (cores[i]->isDone()) {
				confReg->setDone(i);
				if (!dbg2[i]) {
					fprintf(stderr, "%d done core %d\n", machine->num_cycles, i);
				dbg2[i] = true;
			}
			}
		} else if (confReg->isDoneClr(i) & cleared[i]) {
			cores[i]->reset();
			cleared[i] = true;
		} else if (!confReg->isDoneClr(i) & cleared[i]) {
			cleared[i] = false;
		}
	}

	wbuf_arb->step();

}

void SNACC::core_reset()
{
	for (int i = 0; i < core_count; i++) {
		cores[i]->reset();
	}
}

