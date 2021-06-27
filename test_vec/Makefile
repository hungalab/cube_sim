# Makefile to drive test for each teshbench application
# Copyright (c) 2021 Amano laboratory, Keio University.
#     Author: Takuya Kojima

# This file is part of CubeSim, a cycle accurate simulator for 3-D stacked system.

# CubeSim is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.

# CubeSim is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with CubeSim.  If not, see <https://www.gnu.org/licenses/>.

SIMULATOR = ../cube_sim
SIMFLAGS = -F ../vmipsrc
TIMEOUT_SEC = 10
ifeq ($(which timeout), )
SIM = $(SIMULATOR) $(SIMFLAGS)
else
SIM = timeout -sKILL $(TIMEOUT_SEC) $(SIMULATOR) $(SIMFLAGS)
endif

TEST_BENCH = test-adpcm test-aes test-bf test-cp_test test-gsm\
			 test-jpeg test-mpeg2 test-printf test-sha test-sum_asm test-router\
			 test-snacc-mad test-snacc-core test-cma test-dmac \
			 test-snacc-mad-bus test-snacc-core-bus test-cma-bus

all: $(TEST_BENCH)

test-adpcm: adpcm.bin
	$(SIM) -o system_mode=cpu_only $<
test-aes: aes.bin
	$(SIM) -o system_mode=cpu_only $<
test-bf: bf.bin
	$(SIM) -o system_mode=cpu_only $<
test-cp_test: cp_test.bin
	$(SIM) -o system_mode=cpu_only $<
test-gsm: gsm.bin
	$(SIM) -o system_mode=cpu_only $<
test-jpeg: jpeg.bin
	$(SIM) -o system_mode=cpu_only $<
test-mpeg2: mpeg2.bin
	$(SIM) -o system_mode=cpu_only $<
test-printf: printf.bin
	$(SIM) -o system_mode=cpu_only $<
test-sha: sha.bin
	$(SIM) -o system_mode=cpu_only $<
test-sum_asm: sum_asm.bin
	$(SIM) -o system_mode=cpu_only $<
test-router: router_test.bin
	$(SIM) -o accelerator0=RemoteRam -o accelerator1=RemoteRam -o routermsg $<
test-snacc-core: snacc_sum.bin
	$(SIM) -o accelerator0=SNACC  $<
test-snacc-mad: snacc_test.bin
	$(SIM) -o accelerator0=SNACC  $<
test-cma: cma_gray.bin
	$(SIM) -o accelerator0=CMA  $<
test-dmac: dmac.bin
	$(SIM) $<
test-snacc-core-bus: snacc_sum.bin
	$(SIM) -o accelerator0=SNACC -o system_mode=bus_conn  $<
test-snacc-mad-bus: snacc_test.bin
	$(SIM) -o accelerator0=SNACC -o system_mode=bus_conn $<
test-cma-bus: cma_gray.bin
	$(SIM) -o accelerator0=CMA -o system_mode=bus_conn $<