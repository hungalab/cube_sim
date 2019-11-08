SIMULATOR = ../cube_sim
SIMFLAGS = -F ../vmipsrc
SIM = $(SIMULATOR) $(SIMFLAGS)

TEST_BENCH = test-adpcm test-aes test-bf test-cp_test test-gsm\
			 test-jpeg test-mpeg2 test-printf test-sha

all: $(TEST_BENCH)

test-adpcm: adpcm.bin
	$(SIM) $<
test-aes: aes.bin
	$(SIM) $<
test-bf: bf.bin
	$(SIM) $<
test-cp_test: cp_test.bin
	$(SIM) $<
test-gsm: gsm.bin
	$(SIM) $<
test-jpeg: jpeg.bin
	$(SIM) $<
test-mpeg2: mpeg2.bin
	$(SIM) $<
test-printf: printf.bin
	$(SIM) $<
test-sha: sha.bin
	$(SIM) $<