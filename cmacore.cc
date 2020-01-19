#include "cmacore.h"

void CMACore::step()
{
	if (top_module->ctrl_reg->getRun()) {
		count++;
	} else {
		count = 0;
	}
	if (count == result_stat[context].wait) {
		for (int i = 0; i < result_stat[context].len; i++) {
			if (result_stat[context].bank0) {
				bus->store_word(result_stat[context].result_addr + 4 * i, result_stat[context].result[i]);
			} else {
				bus->store_word(0x1000 + result_stat[context].result_addr + 4 * i, result_stat[context].result[i]);
			}
		}
		top_module->done_signal(false);
		context++;
	}
}

void CMACore::reset()
{
	count = 0;
	context = 0;
	result_stat[0] = {0x02C0, true, &comp_y[0], 74, 176};
	result_stat[1] = {0x02C0, false, &comp_y[176], 74, 176};
	result_stat[2] = {0x02C0, true, &comp_cr[0], 74, 176};
	result_stat[3] = {0x02C0, false, &comp_cr[176], 74, 176};
	result_stat[4] = {0x02C0, true, &comp_cb[0], 74, 176};
	result_stat[5] = {0x02C0, false, &comp_cb[176], 74, 176};
	result_stat[6] = {0x40, true, &comp_dct[0], 31, 64};
	result_stat[7] = {0x40, false, &comp_dct[64], 31, 64};
	result_stat[8] = {0x40, true, &comp_dct[128], 31, 64};
	result_stat[9] = {0x40, false, &comp_dct[192], 31, 64};
	result_stat[10] = {0x40, true, &comp_dct[256], 31, 64};
	result_stat[11] = {0x40, false, &comp_dct[320], 31, 64};
	result_stat[12] = {0x40, true, &comp_dct[384], 31, 64};
	result_stat[13] = {0x40, false, &comp_dct[448], 31, 64};
	result_stat[14] = {0x40, true, &comp_dct[512], 31, 64};
	result_stat[15] = {0x40, false, &comp_dct[576], 31, 64};
	result_stat[16] = {0x40, true, &comp_dct[640], 31, 64};
	result_stat[17] = {0x40, false, &comp_dct[704], 31, 64};
	result_stat[18] = {0x300, true, &comp_dct_quant[0], 31, 64};
	result_stat[19] = {0x300, true, &comp_dct_quant[64], 31, 64};
	result_stat[20] = {0x300, true, &comp_dct_quant[128], 31, 64};
	result_stat[21] = {0x300, true, &comp_dct_quant[192], 31, 64};
	result_stat[22] = {0x300, true, &comp_dct_quant[256], 31, 64};
	result_stat[23] = {0x300, true, &comp_dct_quant[320], 31, 64};
	result_stat[24] = {0x300, true, &comp_dct_quant[384], 31, 64};
	result_stat[25] = {0x300, true, &comp_dct_quant[448], 31, 64};
	result_stat[26] = {0x300, true, &comp_dct_quant[512], 31, 64};
	result_stat[27] = {0x300, true, &comp_dct_quant[576], 31, 64};
	result_stat[28] = {0x300, true, &comp_dct_quant[640], 31, 64};
	result_stat[29] = {0x300, true, &comp_dct_quant[704], 31, 64};


}
