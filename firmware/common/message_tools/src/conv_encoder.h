/* -*- c -*- */
/*
 * Copyright 2016-2018 Cognosos, Inc.
 * Author: Sean Nowlan <sean.nowlan@cognosos.com>
 */

#ifndef __CONV_ENCODER_H__
#define __CONV_ENCODER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "bin_utils.h"

// conv_encoder basic object
typedef struct conv_encoder_s {
    unsigned int genpoly;
    unsigned int k;
    unsigned char out_tab[2][(1 << (7 - 1)) * 2]; // TCP_72_40 has k = 7, so 2**17 = 131072 states
    unsigned char trans_tab[2][(1 << (7 - 1)) * 2]; // TCP_72_40 has k = 7, so 2**17 = 131072 states
    unsigned char tail_tab[(1 << (7 - 1))]; // TCP_72_40 has k = 7, so 2**17 = 131072 states
} conv_encoder;

// Create a conv_encoder object.
//  _genpoly        :   generator polynomial
//  _k              :   constraint length of polynomial (must be <= 9)
conv_encoder conv_encoder_create(unsigned int _genpoly, unsigned int _k);

// Destroy conv_encoder object.
//  _q              :   conv_encoder object
void conv_encoder_destroy(void);

// Get a conv_encoder object's constraint length.
//  _q              :   conv_encoder object
unsigned int conv_encoder_get_constraint_length(void);

// Get a conv_encoder object's number of states.
//  _q              :   conv_encoder object
unsigned int conv_encoder_get_num_states(void);

// Get the output length of a message to be encoded by a conv_encoder.
//  _q              :   conv_encoder object
//  _length         :   length of uncoded message
unsigned int conv_encoder_get_output_length(unsigned int _length);

// Encode a message.
// _out buffer must have appropriate length: _length + constraint_length - 1
//  _q              :   conv_encoder object
//  _in             :   input byte array with one bit in LSB position
//  _out            :   output byte array with one bit in LSB position
//  _length         :   length of input array
void conv_encoder_encode(const unsigned char * _in, unsigned char * _out, unsigned int _length);

// Generate an output bit and next state given a particular input bit and current state.
//  _next_state     :   pointer to next state
//  _state_in       :   input state
//  _bit_in         :   input bit
//  _genpoly        :   generator polynomial
//  _k              :   constraint length of polynomial
unsigned char rsc_enc_bit(unsigned char * _next_state, unsigned char _state_in, unsigned char _bit_in, unsigned int _genpoly, unsigned int _k);

// Generate output table and state transition table given a particular input bit.
// _out_tab and _trans_tab buffers must have appropriate lengths!
//  _out_tab        :   output table
//  _trans_tab      :   state transition table
//  _bit_in         :   input bit
//  _genpoly        :   generator polynomial
//  _k              :   constraint length of polynomial
void rsc_transit(unsigned char * _out_tab, unsigned char * _trans_tab, unsigned char _bit_in, unsigned int _genpoly, unsigned int _k);

// Generate tail bits for 2**(k-1) possible states.
// _tailbits buffer must have appropriate length!
//  _tailbits       :   output byte array with one bit in LSB position
//  _genpoly        :   generator polynomial
//  _k              :   constraint length of polynomial
void rsc_tail(unsigned char * _tailbits, unsigned int _genpoly, unsigned int _k);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // #ifndef __CONV_ENCODER_H__
