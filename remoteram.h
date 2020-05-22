#ifndef _REMOTERAM_H_
#define _REMOTERAM_H_

#include <stdio.h>
#include "accelerator.h"
#include "memorymodule.h"

class CubeAccelerator;
class MemoryModule;

class RemoteRam : public CubeAccelerator {
private:
	MemoryModule *mem;

public:
	RemoteRam(uint32 node_ID, Router* upperRouter, int mem_size);
	~RemoteRam();

	void setup();
	void core_step() {};
	void core_reset() {};

	const char *accelerator_name() { return "RemoteRam"; }

	virtual void send_commnad(uint32 cmd, uint32 arg) {};
	virtual bool isTriggered() { return false; };
	virtual uint32 get_dbg_data() { return 0; };

};


#endif //_REMOTERAM_H_
