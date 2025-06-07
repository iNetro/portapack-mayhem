/* -*- c -*- */
/*
 * Copyright 2018 Cognosos, Inc.
 * Author: Sean Nowlan <sean.nowlan@cognosos.com>
 */

#ifndef __CRC16_H__
#define __CRC16_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// crc16 basic object
typedef struct crc16_s {
    uint16_t    poly;       // polynomial mask
    uint16_t    rem;        // initial remainder/seed
    uint16_t    final_xor;  // final XOR value

    uint16_t    reg;        // running checksum register
} crc16;
// Create a crc16 object.
//  _poly           :   polynomial mask
//  _rem            :   initial remainder
//  _final_xor      :   final value XORed with remainder
crc16 crc16_create(uint16_t _poly,
                   uint16_t _rem,
                   uint16_t _final_xor);

// Create a crc16 object with defaults.
//  poly      = 0x1021  (x^16 + x^12 + x^5 + 1)
//  rem       = 0xFFFF
//  final_xor = 0xFFFF
crc16 crc16_create_default(void);

// Destroy crc16 object.
//  _q              :   crc16 object
void crc16_destroy(void);

// Reset crc16 object.
//  _q              :   crc16 object
void crc16_reset(void);

// Get current checksum value of crc16 object.
//  _q              :   crc16 object
uint16_t crc16_checksum(void);

// Update crc16 object with a new input byte.
//  _q              :   crc16 object
//  _b              :   input byte
void crc16_update(uint8_t _b);

// Process array of bytes to update crc16 object.
//  _q              :   crc16 object
//  _in             :   input array
//  _len            :   input array length
void crc16_process(const uint8_t *_in, unsigned int _len);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // #ifndef __CRC16_H__
