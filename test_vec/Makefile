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
			 test-snacc-mad test-snacc-core test-cma test-dmac

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