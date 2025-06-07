/* -*- c -*- */
/*
 * Copyright 2018 Cognosos, Inc.
 * Author: Sean Nowlan <sean.nowlan@cognosos.com>
 */

#include "lfsr.h"
#include "bin_utils.h"

#include <stdlib.h>

#define LFSR_DEF_MASK   0x21    // x^9 + x^5 + 1
#define LFSR_DEF_SEED   0x1ff
#define LFSR_DEF_ORDER  9

static lfsr q;

// Create lfsr object.
lfsr lfsr_create(uint32_t _mask,
                 uint32_t _seed,
                 uint16_t _order)
{
    q.mask  = _mask;
    q.seed  = _seed;
    q.order = _order;
    q.shift = _order - 1;

    lfsr_reset();

    return q;
}

// Create lfsr object with defaults.
lfsr lfsr_create_default(void)
{
    q.mask  = LFSR_DEF_MASK;
    q.seed  = LFSR_DEF_SEED;
    q.order = LFSR_DEF_ORDER;
    q.shift = LFSR_DEF_ORDER - 1;

    lfsr_reset();

    return q;
}

// Destroy lfsr object.
void lfsr_destroy(void)
{
}

// Reset lfsr object.
void lfsr_reset(void)
{
    q.reg = q.seed;
}

// Get sequence length.
uint32_t lfsr_length(void)
{
    return (1 << q.order) - 1;
}

// Get next bit.
uint8_t lfsr_next_bit(void)
{
    uint8_t out_bit = q.reg & 0x1;
    uint8_t new_bit = popcount32(q.reg & q.mask) & 0x1;
    q.reg = (q.reg >> 1) | (new_bit << q.shift);
    return out_bit;
}

// Whiten a byte.
uint8_t lfsr_whiten_byte(uint8_t _b)
{
    uint8_t b = 0;
    for (unsigned int i = 0; i < 8; i++)
    {
        b = (b << 1) | (lfsr_next_bit() & 0x1);
    }
    return _b ^ b;
}

// Whiten array of bytes.
void lfsr_whiten_bytes(const uint8_t *_in, uint8_t *_out, unsigned int _len)
{
    for (unsigned int i = 0; i < _len; i++)
    {
        _out[i] = lfsr_whiten_byte(_in[i]);
    }
}
