/* -*- c -*- */
/*
 * Copyright 2016,2018 Cognosos, Inc.
 * Author: Sean Nowlan <sean.nowlan@cognosos.com>
 */

#ifndef __TPC_ENCODER_H__
#define __TPC_ENCODER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "conv_encoder.h"

#define TPC_NUM_SCHEMES 8
#define TPC_NUM_PARAMS 10

// available TPC schemes
typedef enum {
    TPC_UNKNOWN = 0,  // unknown TPC scheme
    TPC_NONE,         // no TPC
    TPC_12_6,         // TPC(12,6)
    TPC_24_12,        // TPC(24,12)
    TPC_36_18,        // TPC(36,18)
    TPC_48_23,        // TPC(48,23)
    TPC_60_31,        // TPC(60,31)
    TPC_72_40         // TPC(72,40)
} tpc_scheme;

typedef enum {
    TPC_N = 0,  // number of encoded output bytes
    TPC_K,      // number of uncoded input bytes
    TPC_GROW,   // generator polynomial mask for row encoder
    TPC_GCOL,   // generator polynomial mask for column encoder
    TPC_KROW,   // number of uncoded bits per row
    TPC_KCOL,   // number of uncoded bits per column
    TPC_B,      // number of zero pad bits removed prior to transmission
    TPC_Q       // number of zero pad bits sent at start of transmission
} tpc_param;

// TPC parameter table
extern const unsigned char tpc_param_table[TPC_NUM_SCHEMES][TPC_NUM_PARAMS];

// tpc_encoder basic object
typedef struct tpc_encoder_s {
    // options
    tpc_scheme scheme;  // TPC scheme

    // parameters
    unsigned char n;     // number of encoded output bytes
    unsigned char k;     // number of uncoded input bytes
    unsigned char grow;  // generator polynomial mask for row encoder
    unsigned char gcol;  // generator polynomial mask for column encoder
    unsigned char krow;  // number of uncoded bits per row
    unsigned char kcol;  // number of uncoded bits per column
    unsigned char b;     // number of zero pad bits removed prior to transmission
    unsigned char q;     // number of zero pad bits sent at start of transmission

    // derived values
    unsigned char gkrow;   // constraint length of row encoder
    unsigned char gkcol;   // constraint length of column encoder
    unsigned int in_len;   // size of input bit array
    unsigned int out_len;  // size of output bit array
    unsigned int tmp_len;  // size of small working buffer

    unsigned char* in_bits;   // large buffer for input bit array
    unsigned char* out_bits;  // large buffer for output bit array
    unsigned char* tmp_bits;  // small buffer for conv_encoders

    conv_encoder row_enc;  // convolutional encoder for rows
    conv_encoder col_enc;  // convolutional encoder for columns
} tpc_encoder;

// Create a tpc_encoder object.
//  _scheme         :   TPC scheme
tpc_encoder tpc_encoder_create(tpc_scheme _scheme);

// Destroy tpc_encoder object.
//  _q              :   tpc_encoder object
void tpc_encoder_destroy(void);

// Reset tpc_encoder object.
//  _q              :   tpc_encoder object
void tpc_encoder_reset(void);

// Get a tpc_decoder object's input length in bytes.
//  _q              :   tpc_scheme object
unsigned int tpc_decoder_get_input_length_bytes(const tpc_scheme scheme);

// Get a tpc_decoder object's output length in bytes.
//  _q              :   tpc_scheme object
unsigned int tpc_decoder_get_output_length_bytes(const tpc_scheme scheme);

// Get a tpc_encoder object's input length in bytes.
//  _q              :   tpc_encoder object
unsigned int tpc_encoder_get_input_length_bytes(void);

// Get a tpc_encoder object's output length in bytes.
//  _q              :   tpc_encoder object
unsigned int tpc_encoder_get_output_length_bytes(void);

// Get a tpc_encoder object's input length in bits.
//  _q              :   tpc_encoder object
unsigned int tpc_encoder_get_input_length(void);

// Get a tpc_encoder object's output length in bits.
//  _q              :   tpc_encoder object
unsigned int tpc_encoder_get_output_length(void);

// Encode a message.
// _out buffer must have appropriate length!
//  _q              :   tpc_encoder object
//  _in             :   input byte array
//  _out            :   output byte array
void tpc_encoder_encode(const unsigned char* _in, unsigned char* _out);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // #ifndef __TPC_ENCODER_H__
