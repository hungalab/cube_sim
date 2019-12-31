#include <stdio.h>
#include "routerinterface.h"

RouterInterface::RouterInterface() : DeviceMap(0x1000000) {
}

void RouterInterface::step() { return; }

void RouterInterface::reset() { return; }