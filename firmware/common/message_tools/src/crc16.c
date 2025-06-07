/* -*- c -*- */
/*
 * Copyright 2018 Cognosos, Inc.
 * Author: Sean Nowlan <sean.nowlan@cognosos.com>
 */

#include "crc16.h"

#include <stdlib.h>

#define CRC16_DEF_POLY      0x1021  // x^16 + x^12 + x^5 + 1
#define CRC16_DEF_REM       0xffff
#define CRC16_DEF_FINAL_XOR 0xffff

static crc16 q;

// Create crc16 object.
crc16 crc16_create(uint16_t _poly,
                   uint16_t _rem,
                   uint16_t _final_xor)
{
    q.poly      = _poly;
    q.rem       = _rem;
    q.final_xor = _final_xor;

    crc16_reset();

    return q;
}

// Create crc16 object with defaults.
crc16 crc16_create_default(void)
{
    q.poly      = CRC16_DEF_POLY;
    q.rem       = CRC16_DEF_REM;
    q.final_xor = CRC16_DEF_FINAL_XOR;

    crc16_reset();

    return q;
}

// Destroy crc16 object.
void crc16_destroy(void)
{
}

// Reset crc16 object.
void crc16_reset(void)
{
    q.reg = q.rem;
}

// Get checksum.
uint16_t crc16_checksum(void)
{
    return q.reg ^ q.final_xor;
}

// Update crc16 object with new byte.
void crc16_update(uint8_t _b)
{
    for (unsigned int i = 0; i < 8; i++)
    {
        if (((q.reg & 0x8000) >> 8) ^ (_b & 0x80))
        {
            q.reg <<= 1;          // shift left once
            q.reg ^= q.poly;    // XOR with truncated polynomial
        }
        else
        {
            q.reg <<= 1;          // shift left once
        }

        _b <<= 1;                   // next data bit
    }
}

// Update crc16 object with new array of bytes.
void crc16_process(const uint8_t *_in, unsigned int _len)
{
    for (unsigned int i = 0; i < _len; i++)
    {
        crc16_update(_in[i]);
    }
}
