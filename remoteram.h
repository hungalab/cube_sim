#ifndef _REMOTERAM_H_
#define _REMOTERAM_H_

#include <stdio.h>
#include "accelerator.h"

class CubeAccelerator;

class RemoteRam : public CubeAccelerator {
private:

public:
	RemoteRam(uint32 node_ID, Router* upperRouter) : CubeAccelerator(node_ID, upperRouter) {};
	~RemoteRam() {};

	void setup() { fprintf(stderr, "RemoteRam setting up\n"); };

	const char *accelerator_name() { return "RemoteRam"; }
};


#endif //_REMOTERAM_H_
