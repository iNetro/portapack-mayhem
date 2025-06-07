/* -*- c -*- */
/*
 * Copyright 2016-2018 Cognosos, Inc.
 * Author: Sean Nowlan <sean.nowlan@cognosos.com>
 */

#ifndef __BIN_UTILS_H__
#define __BIN_UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// the following parameters match the v2.x hardware CRC16 configuration
#define CRCPOLY 0x1021  // truncated polynomial: x^16 + x^12 + x^5 + 1
#define CRCSEED 0xFFFF  // initial remainder/seed
//#define CRCSEED 0x1D0F  // initial remainder/seed
#define CRCXOR  0xFFFF  // final XOR value

// Calculate the number of '1' bits in a 32-bit unsigned int.
//  _x              :   value
unsigned int popcount32(unsigned int _x);

// Unpack a byte array of packed bytes with 8 significant bits to a byte array
// containing unpacked bits with 1 bit in LSB position. Bits are read from MSB
// to LSB.
// _out buffer must have appropriate length of 8*_length!
//  _in             :   input byte array
//  _out            :   output byte array
//  _length         :   number of bytes to unpack
void unpack(const unsigned char * _in, unsigned char * _out, unsigned int _length);

// Pack a byte array containing unpacked bits with 1 bit in LSB position to a
// byte array of packed bytes with 8 significant bits. Bits are written from
// MSB to LSB.
// _out buffer must have appropriate length of _length!
//  _in             :   input byte array
//  _out            :   output byte array
//  _length         :   number of bytes to pack
void pack(const unsigned char * _in, unsigned char * _out, unsigned int _length);

// compute a checksum using the CRC16-CCITT algorithm
//  _in             : input byte array
//  _length         : length of input array
uint16_t crc16_ccitt(const uint8_t * _in, uint16_t _length);

// Set a specific bit in a byte array
// Use both byte index and bit index, bit index >= 8 goes to byte index
// *** bit_index start from MSB ***
// array		:byte array to set a specific bit
// byte_index	:byte shift
// bit_index	:which bit to set
bool byte_array_set_bit(uint8_t *array, uint32_t byte_index, uint32_t bit_index, bool b);

// Retrieve a specific bit in a byte array
// Use both byte index and bit index, bit index >= 8 goes to byte index
// *** bit_index start from MSB ***
// array		:byte array to get a specific bit
// byte_index	:byte shift
// bit_index	:which bit to get.
// return		:if bit is 0 return false, if 1 return true;
bool byte_array_get_bit(const uint8_t *array, uint32_t byte_index, uint32_t bit_index);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // #ifndef __BIN_UTILS_H__
