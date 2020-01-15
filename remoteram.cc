#include "remoteram.h"

RemoteRam::RemoteRam(uint32 node_ID, Router* upperRouter, int mem_size)
	: CubeAccelerator(node_ID, upperRouter)
{
	mem = new MemoryModule(mem_size);
}

RemoteRam::~RemoteRam()
{
	delete mem;
}

void RemoteRam::setup()
{
	localBus->map_at_local_address(mem, 0x0);
}