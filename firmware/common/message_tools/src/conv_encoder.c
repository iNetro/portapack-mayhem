/* -*- c -*- */
/*
 * Copyright 2016,2018 Cognosos, Inc.
 * Author: Sean Nowlan <sean.nowlan@cognosos.com>
 */

#include "bin_utils.h"
#include "conv_encoder.h"

#include <stdlib.h>

static conv_encoder q;

// Create a conv_encoder object.
conv_encoder conv_encoder_create(unsigned int _genpoly, unsigned int _k)
{
    // Hardcoded values for k
    _k = 7; // TCP_72_40
    _genpoly = 123; // TPC_72_40;

    unsigned int i;

    q.genpoly = _genpoly;
    q.k = _k;

    // populate output table and state transition table
    for(i = 0; i < 2; i++) {
        rsc_transit(q.out_tab[i], q.trans_tab[i], i, q.genpoly, q.k);
    }

    // populate tail bits table
    rsc_tail(q.tail_tab, q.genpoly, q.k);

    return q;
}

// Destroy conv_encoder object.
void conv_encoder_destroy(void)
{
}

// Get a conv_encoder object's constraint length.
unsigned int conv_encoder_get_constraint_length(void){
    return q.k;
}

// Get a conv_encoder object's number of states.
unsigned int conv_encoder_get_num_states(void)
{
    return 1 << (q.k - 1);
}

// Get the output length of a message to be encoded by a conv_encoder.
unsigned int conv_encoder_get_output_length(unsigned int _length)
{
    return _length + q.k - 1;
}

// Encode a message.
void conv_encoder_encode(const unsigned char * _in, unsigned char * _out, unsigned int _length)
{
    unsigned int i;
    unsigned int m = q.k - 1;
    unsigned char state = 0;
    unsigned char bit;

    // encode data bits
    for(i = 0; i < _length; i++) {
        bit = _in[i];                       // save input bit in case _in == _out
        _out[i] = q.out_tab[bit][state];  // systematic output bit
        state = q.trans_tab[bit][state];  // look up next state
    }

    // encode tail bits
    for(i = _length; i < _length + m; i++) {
        bit = q.tail_tab[state];
        _out[i] = q.out_tab[bit][state];
        state = q.trans_tab[bit][state];
    }
}

// Generate an output bit and next state given a particular input bit and current state.
unsigned char rsc_enc_bit(unsigned char * _next_state, unsigned char _state_in, unsigned char _bit_in, unsigned int _genpoly, unsigned int _k)
{
    unsigned char bit_out = _bit_in;                                                // systematic output
    unsigned char fb = (_bit_in ^ (popcount32(_state_in & _genpoly) & 0x1)) & 0x1;  // feedback bit

    *_next_state = (_state_in ^ (fb << (_k - 1))) >> 1;                             // next state

    return bit_out;
}

// Generate output table and state transition table given a particular input bit.
void rsc_transit(unsigned char * _out_tab, unsigned char * _trans_tab, unsigned char _bit_in, unsigned int _genpoly, unsigned int _k)
{
    unsigned char state, next_state;
    unsigned int num_states = 1 << (_k - 1);

    for(state = 0; state < num_states; state++) {
        _out_tab[state] = rsc_enc_bit(&next_state, state, _bit_in, _genpoly, _k);
        _trans_tab[state] = next_state;
    }
}

// Generate tail bits for 2**(k-1) possible states.
void rsc_tail(unsigned char * _tailbits, unsigned int _genpoly, unsigned int _k)
{
    unsigned char state;
    unsigned int num_states = 1 << (_k - 1);

    for(state = 0; state < num_states; state++) {
        _tailbits[state] = popcount32(state & _genpoly) & 0x1;
    }
}
