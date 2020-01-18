#ifndef _ACCELERATORCORE_H_
#define _ACCELERATORCORE_H_

#include "accelerator.h"

class AcceleratorCore {
private:

public:

	virtual void step() = 0;
	virtual void reset() = 0;
};


#endif //_ACCELERATORCORE_H_