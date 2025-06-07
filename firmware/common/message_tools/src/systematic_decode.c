#include "bin_utils.h"
#include "systematic_decode.h"

#include <math.h>
#include <stdlib.h>

void systematic_decode(tpc_scheme _scheme,   // TPC scheme
                  const uint8_t * in,   // input buffer of length n
                  uint8_t * out)        // output buffer of length k
{
    uint8_t n    = tpc_param_table[_scheme][TPC_N];
    uint8_t k    = tpc_param_table[_scheme][TPC_K];
    uint8_t grow = tpc_param_table[_scheme][TPC_GROW];
    uint8_t gcol = tpc_param_table[_scheme][TPC_GCOL];
    uint8_t krow = tpc_param_table[_scheme][TPC_KROW];
    uint8_t kcol = tpc_param_table[_scheme][TPC_KCOL];
    uint8_t b    = tpc_param_table[_scheme][TPC_B];
    uint8_t q    = tpc_param_table[_scheme][TPC_Q];

    uint16_t count = 0;
    uint16_t gkrow = ceil(log2(grow));
    uint16_t gkcol = ceil(log2(gcol));
    uint16_t row_len = krow + gkrow - 1;
    uint16_t col_len = kcol + gkcol - 1;

	// handle first row: skip Q padding bits that were transmitted
	// B padding bits were NOT transmitted, NOT in tx data in[],
    // Assume TCP scheme is correct, B is always smaller than krow
    // handle all rows including first row
    uint16_t i = 0, j = b + q;
    for (; i < kcol; i++)
    {
    	for (; j < krow; j++)
    	{
    		byte_array_set_bit(
    				out, 0, count++,
					byte_array_get_bit(
							in, 0, i * row_len + j - b));
    	}
    	j = 0;
    }
}
