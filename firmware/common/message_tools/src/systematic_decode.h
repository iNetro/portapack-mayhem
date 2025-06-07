/* -*- c -*- */
/*
 * Copyright 2019 Cognosos, Inc.
 * Author: Sean Nowlan <sean.nowlan@cognosos.com>
 */

#ifndef __SYSTEMATIC_DECODE_H__
#define __SYSTEMATIC_DECODE_H__

#include "tpc_encoder.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "tpc_encoder.h"

// Decode systematic bits from a TPC encoded message.
//  _scheme         :   TPC scheme
//  _in             :   input byte array of length n
//  _out            :   output byte array of length k
void systematic_decode(tpc_scheme _scheme, const uint8_t * in, uint8_t * out);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // #ifndef __SYSTEMATIC_DECODE_H__
