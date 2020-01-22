#ifndef _ACCELERATORCORE_H_
#define _ACCELERATORCORE_H_

typedef std::function<void(bool)> SIGNAL_PTR;
#include "accelerator.h"

class LocalMapper;
class CubeAccelerator;

class AcceleratorCore {
private:

protected:
	LocalMapper *bus;
	SIGNAL_PTR done_signal;

public:
	AcceleratorCore(LocalMapper *bus_, SIGNAL_PTR done_signal_)
	: bus(bus_), done_signal(done_signal_) {};
	virtual void step() = 0;
	virtual void reset() = 0;
};


#endif //_ACCELERATORCORE_H_