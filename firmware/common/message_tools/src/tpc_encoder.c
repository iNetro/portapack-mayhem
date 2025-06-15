/* -*- c -*- */
/*
 * Copyright 2016,2018 Cognosos, Inc.
 * Author: Sean Nowlan <sean.nowlan@cognosos.com>
 */

#include "bin_utils.h"
#include "conv_encoder.h"
#include "tpc_encoder.h"

#include <math.h>
#include <stdlib.h>

const unsigned char tpc_param_table[TPC_NUM_SCHEMES][TPC_NUM_PARAMS] = {
    // n,     k,      grow,   gcol,   krow,   kcol,   b,      q
    {1, 1, 1, 1, 8, 1, 0, 0},
    {1, 1, 1, 1, 8, 1, 0, 0},
    {12, 6, 3, 123, 3, 18, 0, 6},
    {24, 12, 3, 43, 17, 6, 6, 0},
    {36, 18, 3, 43, 26, 6, 9, 3},
    {48, 23, 123, 43, 22, 9, 8, 6},
    {60, 31, 123, 123, 16, 16, 4, 4},
    {72, 40, 123, 123, 18, 18, 0, 4}};

static tpc_encoder q;

// Create tpc_encoder object.
tpc_encoder tpc_encoder_create(tpc_scheme _scheme) {
    q.scheme = _scheme;
    q.n = tpc_param_table[_scheme][TPC_N];
    q.k = tpc_param_table[_scheme][TPC_K];
    q.grow = tpc_param_table[_scheme][TPC_GROW];
    q.gcol = tpc_param_table[_scheme][TPC_GCOL];
    q.krow = tpc_param_table[_scheme][TPC_KROW];
    q.kcol = tpc_param_table[_scheme][TPC_KCOL];
    q.b = tpc_param_table[_scheme][TPC_B];
    q.q = tpc_param_table[_scheme][TPC_Q];

    // derived values
    q.gkrow = ceil(log(q.grow) / log(2));
    q.gkcol = ceil(log(q.gcol) / log(2));
    q.in_len = 8 * q.k + q.b + q.q;                               // input length plus padding size
    q.out_len = (q.krow + q.gkrow - 1) * (q.kcol + q.gkcol - 1);  // size of encoded matrix
    q.tmp_len = q.kcol + q.gkcol - 1;                             // column length including encoded bits

    // allocate buffers
    static uint8_t buffer_out[600];
    static uint8_t buffer_in[8 * 40 + 0 + 4];  // 40 bytes for TPC_72_40, plus 0 + 4 for b + q pad bits
    static uint8_t buffer_tmp[18 + 7 - 1];     // 123 bits for TPC_72_40, plus 7 - 1 for gkcol

    q.in_bits = buffer_in;
    q.out_bits = buffer_out;
    q.tmp_bits = buffer_tmp;

    // create row and column encoders
    q.row_enc = conv_encoder_create(q.grow, q.gkrow);
    q.col_enc = conv_encoder_create(q.gcol, q.gkcol);

    return q;
}

// Destroy tpc_encoder object.
void tpc_encoder_destroy(void) {
}

// Reset tpc_encoder object.
void tpc_encoder_reset(void) {
    unsigned int i;

    // initialize buffers
    for (i = 0; i < q.in_len; i++) {
        q.in_bits[i] = 0;
    }

    for (i = 0; i < q.out_len; i++) {
        q.out_bits[i] = 0;
    }

    for (i = 0; i < q.tmp_len; i++) {
        q.tmp_bits[i] = 0;
    }
}

// Get a tpc_decoder object's input length in bytes.
unsigned int tpc_decoder_get_input_length_bytes(const tpc_scheme scheme) {
    // return q.n;
    return tpc_param_table[scheme][TPC_N];
}

// Get a tpc_decoder object's output length in bytes.
unsigned int tpc_decoder_get_output_length_bytes(const tpc_scheme scheme) {
    // return q.k;
    return tpc_param_table[scheme][TPC_K];
}

// Get a tpc_encoder object's input length in bytes.
unsigned int tpc_encoder_get_input_length_bytes(void) {
    return q.k;
}

// Get a tpc_encoder object's output length in bytes.
unsigned int tpc_encoder_get_output_length_bytes(void) {
    return q.n;
}

// Get a tpc_encoder object's input length in bits.
unsigned int tpc_encoder_get_input_length(void) {
    return q.k * 8;
}

// Get a tpc_encoder object's output length in bits.
unsigned int tpc_encoder_get_output_length(void) {
    return q.n * 8;
}

// Encode a message.
void tpc_encoder_encode(const unsigned char* _in, unsigned char* _out) {
    unsigned int i, j;
    unsigned int ncols = q.krow + q.gkrow - 1;

    // reset encoder buffers
    tpc_encoder_reset();

    // unpack input buffer to position immediately following (B + Q) pad bits
    unpack(_in, &q.in_bits[q.b + q.q], q.k);

    // encode rows
    for (i = 0; i < q.kcol; i++) {
        conv_encoder_encode(&q.in_bits[q.krow * i], &q.out_bits[ncols * i], q.krow);
    }

    // encode columns
    for (i = 0; i < ncols; i++) {
        // assemble bits from the ith column into tmp_bits buffer
        for (j = 0; j < q.kcol; j++) {
            q.tmp_bits[j] = q.out_bits[ncols * j + i];
        }

        // encode the ith column
        conv_encoder_encode(q.tmp_bits, q.tmp_bits, q.kcol);

        // copy just the parity bits for ith column to output buffer
        for (j = 0; j < q.gkcol - 1; j++) {
            q.out_bits[(q.kcol + j) * ncols + i] = q.tmp_bits[q.kcol + j];
        }
    }

    // pack output buffer, skipping B pad bits that are not transmitted
    pack(&q.out_bits[q.b], _out, q.n);
}
