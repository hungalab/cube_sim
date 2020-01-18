#ifndef _CMACORE_H_
#define _CMACORE_H_

#include "acceleratorcore.h"
#include "cma.h"
#include "accelerator.h"

class AcceleratorCore;
class CMA;
class LocalMapper;

class CMACore : public AcceleratorCore {
private:
	CMA *top_module;
	LocalMapper *bus;
	int count;
public:
	CMACore(CMA *top_module_, LocalMapper *bus_) : top_module(top_module_), bus(bus_) {};
	virtual void step();
	virtual void reset();
};


#endif //_CMACORE_H_