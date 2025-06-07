/* -*- c -*- */
/*
 * Copyright 2018 Cognosos, Inc.
 * Author: Sean Nowlan <sean.nowlan@cognosos.com>
 */

#ifndef __LFSR_H__
#define __LFSR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// basic lfsr object
typedef struct lfsr_s {
    uint32_t mask;  // polynomial mask
    uint32_t seed;  // initial seed value
    uint16_t order; // polynomial order

    uint16_t shift; // shift amount (order - 1)
    uint32_t reg;   // shift register
} lfsr;

// Create an lfsr object.
//  _mask           :   polynomial mask
//  _seed           :   initial value/seed
//  _order          :   polynomial order
lfsr lfsr_create(uint32_t _mask,
                 uint32_t _seed,
                 uint16_t _order);

// Create a lfsr object with defaults.
//  mask  = 0x21 (x^9 + x^5 + 1)
//  seed  = 0x1ff
//  order = 9
lfsr lfsr_create_default(void);

// Destroy lfsr object.
//  _q              :   lfsr object
void lfsr_destroy(void);

// Reset lfsr object.
//  _q              :   lfsr object
void lfsr_reset(void);

// Get sequence length.
//  _q              :   lfsr object
uint32_t lfsr_length(void);

// Get next bit in LFSR sequence.
//  _q              :   lfsr object
uint8_t lfsr_next_bit(void);

// Whiten input byte.
//  _q              :   lfsr object
//  _b              :   input byte
uint8_t lfsr_whiten_byte(uint8_t _b);

// Whiten array of input bytes.
//  _q              :   lfsr object
//  _in             :   input array
//  _out            :   output array
//  _len            :   array size
void lfsr_whiten_bytes(const uint8_t *_in, uint8_t *_out, unsigned int _len);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // #ifndef __LFSR_H__
