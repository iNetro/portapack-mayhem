/* -*- c -*- */
/*
 * Copyright 2016,2018 Cognosos, Inc.
 * Author: Sean Nowlan <sean.nowlan@cognosos.com>
 */
#include "bin_utils.h"

// Calculate the number of '1' bits in a 32-bit unsigned int.
//  Adapted from pop2(...) in Hacker's Delight:
//  http://www.hackersdelight.org/hdcodetxt/pop.c.txt
unsigned int popcount32(unsigned int x)
{
	unsigned int r = x - ((x >> 1) & 033333333333) - ((x >> 2) & 011111111111);
    r = (r + (r >> 3)) & 030707070707;
    return ((r * 0404040404) >> 26) + (r >> 30);
}

// Unpack a byte array of packed bytes to a byte array of unpacked bits.
void unpack(const unsigned char * _in, unsigned char * _out, unsigned int _length)
{
    unsigned int i, j;

    for(i = 0; i < _length; i++) {
        for(j = 0; j < 8; j++) {
            _out[8*i + j] = (_in[i] >> (7-j)) & 0x1;
        }
    }
}

// Pack a byte array of unpacked bits to a byte array of packed bytes.
void pack(const unsigned char * _in, unsigned char * _out, unsigned int _length)
{
    unsigned int i, j;
    unsigned char byte;

    for(i = 0; i < _length; i++) {
        byte = 0;
        for(j = 0; j < 8; j++) {
            byte = (byte << 1) | (_in[8*i + j] & 0x1);
        }
        _out[i] = byte;
    }
}

// compute a checksum using the CRC16-CCITT algorithm
uint16_t crc16_ccitt(const uint8_t * _in, uint16_t _length)
{
    uint16_t crc = CRCSEED;
    uint16_t i;
    uint8_t  j;
    uint8_t  data;

    for (i = 0; i < _length; i++)
    {
        data = _in[i];
        for (j = 0; j < 8; j++)
        {
            if (((crc & 0x8000) >> 8) ^ (data & 0x80))
            {
                crc <<= 1;      // shift left once
                crc ^= CRCPOLY; // XOR with truncated polynomial
            }
            else
            {
                crc <<= 1;      // shift left once
            }
            data <<= 1;         // next data bit
        }
    }

    return crc ^ CRCXOR;        // perform bitwise XOR with CRCXOR
}

// Set a specific bit in a byte array
// *** bit_index start from MSB ***
bool byte_array_set_bit(uint8_t *array, uint32_t byte_index, uint32_t bit_index, bool b)
{
	byte_index += bit_index / 8;
	bit_index %= 8;
	if(b)
	{// set a bit to 1
		array[byte_index] |= (0x01 << (7-bit_index));
	}else {// set a bit to 0
		array[byte_index] &= (~(0x01 << (7-bit_index)));
	}
	return b;
}

// Retrieve a specific bit in a byte array
// *** bit_index start from MSB ***
bool byte_array_get_bit(const uint8_t *array, uint32_t byte_index, uint32_t bit_index)
{
	byte_index += bit_index / 8;
	bit_index %= 8;
	return (bool)((array[byte_index] >> (7-bit_index)) & 0x01);
}

