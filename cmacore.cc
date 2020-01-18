#include "cmacore.h"

void CMACore::step()
{
	if (top_module->ctrl_reg->getRun()) {
		count++;
	} else {
		count = 0;
	}
	if (count == 100) {
		top_module->done_signal(false);
	}
}

void CMACore::reset()
{
	count = 0;
}
