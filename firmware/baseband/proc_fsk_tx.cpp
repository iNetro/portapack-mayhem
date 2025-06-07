/*
 * Copyright (C) 2014 Jared Boone, ShareBrained Technology, Inc.
 * Copyright (C) 2016 Furrtek
 * Copyright (C) 2023 TJ Baginski
 *
 * This file is part of PortaPack.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#include "proc_fsk_tx.hpp"
#include "portapack_shared_memory.hpp"
#include "sine_table_int8.hpp"
#include "event_m4.hpp"

#include <cstdint>

void FSKTxProcessor::encode_radio_packet(uint8_t * pau8_BufferIn, uint16_t u16_InLength, uint16_t * pu16_OutLength)
{
    uint16_t u16_LengthCRC;
    uint16_t u16_TPC_InLength;
    uint16_t u16_TPC_OutLength;
    uint16_t u16_BlockCount;
    uint16_t u16_PadLength;
    uint16_t u16_RemnantLength;
    uint16_t u16_EncodingInLength;
    uint16_t u16_EncoderOutLength;

	// Set default out length
	*pu16_OutLength = 0;

    crc16_create_default();
    lfsr_create_default();
    tpc_encoder_create(TPC_72_40);

	u16_LengthCRC = 2;

	u16_TPC_InLength = (uint16_t)tpc_encoder_get_input_length_bytes();
	u16_TPC_OutLength = (uint16_t)tpc_encoder_get_output_length_bytes();

	u16_BlockCount = (u16_InLength + u16_LengthCRC) / u16_TPC_InLength;

	u16_PadLength = 0;
    u16_RemnantLength = (u16_InLength + u16_LengthCRC) % u16_TPC_InLength;
    if(u16_RemnantLength)
    {
    	u16_BlockCount++;
    	u16_PadLength = u16_TPC_InLength - u16_RemnantLength;
    }

    u16_EncodingInLength = u16_BlockCount * u16_TPC_InLength;
    u16_EncoderOutLength = u16_BlockCount * u16_TPC_OutLength;

    memcpy(encodedDataIn, pau8_BufferIn, u16_InLength);
    memset(encodedDataOut, 0, sizeof(encodedDataOut));
    if(u16_EncodingInLength > u16_InLength)
    {
        memset(encodedDataIn + u16_InLength, 0, (u16_EncodingInLength - u16_InLength));
    }

    uint16_t u16_PayloadLength;
    uint16_t u16_Checksum;

    u16_PayloadLength = u16_InLength + u16_PadLength;
    crc16_reset();
    crc16_process(encodedDataIn, u16_PayloadLength);
    u16_Checksum = crc16_checksum();
    encodedDataIn[u16_PayloadLength] = (u16_Checksum >> 8) & 0xff;   // upper byte
    encodedDataIn[u16_PayloadLength+1] = u16_Checksum & 0xff;   // lower byte

    lfsr_reset();
    lfsr_whiten_bytes(encodedDataIn, encodedDataIn, u16_EncodingInLength);

    for(uint16_t i = 0; i < u16_BlockCount; i++)
    {
        tpc_encoder_reset();
        tpc_encoder_encode(&encodedDataIn[i * u16_TPC_InLength],
							&encodedDataOut[i * u16_TPC_OutLength]);
    }

    *pu16_OutLength = u16_EncoderOutLength;
}

int FSKTxProcessor::gen_sample_from_phy_bit(char* bit, char* sample, int num_bit) {
    int num_sample = (num_bit * SAMPLE_PER_SYMBOL) + (LEN_GAUSS_FILTER * SAMPLE_PER_SYMBOL);

    int8_t* tmp_phy_bit_over_sampling_int8 = (int8_t*)tmp_phy_bit_over_sampling;

    // Zero-padding before and after impulse train
    int i, j;
    for (i = 0; i < (LEN_GAUSS_FILTER * SAMPLE_PER_SYMBOL - 1); i++) {
        tmp_phy_bit_over_sampling_int8[i] = 0;
    }
    for (i = (LEN_GAUSS_FILTER * SAMPLE_PER_SYMBOL - 1 + num_bit * SAMPLE_PER_SYMBOL);
         i < (2 * LEN_GAUSS_FILTER * SAMPLE_PER_SYMBOL - 2 + num_bit * SAMPLE_PER_SYMBOL); i++) {
        tmp_phy_bit_over_sampling_int8[i] = 0;
    }

    // Impulse train creation (NRZ mapping: 0 → -1, 1 → +1)
    for (i = 0; i < (num_bit * SAMPLE_PER_SYMBOL); i++) {
        if (i % SAMPLE_PER_SYMBOL == 0) {
            tmp_phy_bit_over_sampling_int8[i + (LEN_GAUSS_FILTER * SAMPLE_PER_SYMBOL - 1)] = (bit[i / SAMPLE_PER_SYMBOL]) * 2 - 1;
        } else {
            tmp_phy_bit_over_sampling_int8[i + (LEN_GAUSS_FILTER * SAMPLE_PER_SYMBOL - 1)] = 0;
        }
    }

    // Phase accumulator
    int16_t tmp = 0;
    sample[0] = cos_table_int8[tmp];
    sample[1] = sin_table_int8[tmp];

    int len_conv_result = num_sample - 1;
    for (i = 0; i < len_conv_result; i++) {
        int16_t acc = 0;
        // Use all 16 Gaussian taps
        for (j = 0; j < 16; j++) {
            acc += gauss_coef_int8[15 - j] * tmp_phy_bit_over_sampling_int8[i + j];
        }

        tmp = (tmp + acc) & 1023;

        sample[(i + 1) * 2 + 0] = cos_table_int8[tmp];
        sample[(i + 1) * 2 + 1] = sin_table_int8[tmp];
    }

    return num_sample;
}

void FSKTxProcessor::octet_hex_to_bit(char* hex, char* bit) {
    char tmp_hex[3];

    tmp_hex[0] = hex[0];
    tmp_hex[1] = hex[1];
    tmp_hex[2] = 0;

    int n = strtol(tmp_hex, NULL, 16);

    bit[0] = 0x01 & (n >> 0);
    bit[1] = 0x01 & (n >> 1);
    bit[2] = 0x01 & (n >> 2);
    bit[3] = 0x01 & (n >> 3);
    bit[4] = 0x01 & (n >> 4);
    bit[5] = 0x01 & (n >> 5);
    bit[6] = 0x01 & (n >> 6);
    bit[7] = 0x01 & (n >> 7);
}

uint8_t FSKTxProcessor::hex_char_to_nibble(char c) {
    if ('0' <= c && c <= '9') return c - '0';
    else if ('a' <= c && c <= 'f') return c - 'a' + 10;
    else if ('A' <= c && c <= 'F') return c - 'A' + 10;
    else return 0;  // Or handle error
}

uint8_t FSKTxProcessor::hex_pair_to_byte(char high, char low) {
    return (hex_char_to_nibble(high) << 4) | hex_char_to_nibble(low);
}

int FSKTxProcessor::convert_hex_to_bit(char* hex, char* bit, int stream_flip, int octet_limit) {
    int num_hex_orig = strlen(hex);

    int i, num_hex;
    num_hex = num_hex_orig;
    for (i = 0; i < num_hex_orig; i++) {
        if (!((hex[i] >= 48 && hex[i] <= 57) || (hex[i] >= 65 && hex[i] <= 70) || (hex[i] >= 97 && hex[i] <= 102)))  // not a hex
            num_hex--;
    }

    if (num_hex % 2 != 0) {
        return (-1);
    }

    if (num_hex > (octet_limit * 2)) {
        return (-1);
    }
    if (num_hex <= 1) {  // NULL data
        return (-1);
    }

    char tmp_str[max_char];

    if (stream_flip == 1) {
        strcpy(tmp_str, hex);
        for (i = 0; i < num_hex; i = i + 2) {
            hex[num_hex - i - 2] = tmp_str[i];
            hex[num_hex - i - 1] = tmp_str[i + 1];
        }
    }

    int num_bit = num_hex * 4;

    int j;
    for (i = 0; i < num_hex; i = i + 2) {
        j = i * 4;
        octet_hex_to_bit(hex + i, bit + j);
    }

    return (num_bit);
}

int FSKTxProcessor::calculate_sample_for_ADV(PKT_INFO* pkt) {
    pkt->num_phy_bit = 0;

    // gen preamble and access address
    const char* AA = "AAAAAAAA";
    const char* AAValue = "D6BE898E";
    pkt->num_phy_bit = pkt->num_phy_bit + convert_hex_to_bit((char*)AA, pkt->phy_bit, 0, 4);
    pkt->num_phy_bit = pkt->num_phy_bit + convert_hex_to_bit((char*)AAValue, pkt->phy_bit + pkt->num_phy_bit, 0, 4);

    uint16_t u16_OutLength = 0;

    uint8_t advertisementDataByte[32] = {0};

    for (int i = 0; i < 32; i++) {

        advertisementDataByte[i] = hex_pair_to_byte(advertisementData[i * 2], advertisementData[i * 2 + 1]);
    }

   // encode_radio_packet(advertisementDataByte, 32, &u16_OutLength);

    for (int i = 8; i < MAX_NUM_PHY_BYTE; i++) 
    {
        for (int j = 7; j >= 0; j--) 
        {
            pkt->phy_bit[i * 8 + j] = (encodedDataOut[i] >> j) & 0x01;
            pkt->num_phy_bit++;
        }
    }

    pkt->num_phy_sample = gen_sample_from_phy_bit(pkt->phy_bit, pkt->phy_sample, pkt->num_phy_bit);

    return (0);
}

int FSKTxProcessor::calculate_sample_from_pkt_type(PKT_INFO* pkt) {
    // Todo: Handle other Enum values.
    // if (packetType == ADV_IND);

    if (calculate_sample_for_ADV(pkt) == -1) {
        return (-1);
    }

    return (0);
}

int FSKTxProcessor::calculate_pkt_info(PKT_INFO* pkt) {
    if (calculate_sample_from_pkt_type(pkt) == -1) {
        return (-1);
    }

    return (0);
}

void FSKTxProcessor::execute(const buffer_c8_t& buffer) {
    int8_t re, im;

    // This is called at 4M/2048 = 1953Hz
    for (size_t i = 0; i < buffer.count; i++) {
        if (configured) {
            // This is going to loop through each sample bit and push it to the output buffer.
            if (sample_count > length) {
                configured = false;
                sample_count = 0;

                txprogress_message.done = true;
                shared_memory.application_queue.push(txprogress_message);
            } else {
                // Real and imaginary was already calculated in gen_sample_from_phy_bit.
                // It was processed from each data bit, run through a Gaussian Filter, and then ran through sin and cos table to get each IQ bit.
                re = (int8_t)packets.phy_sample[sample_count++];
                im = (int8_t)packets.phy_sample[sample_count++];

                buffer.p[i] = {re, im};

                if (progress_count >= progress_notice) {
                    progress_count = 0;
                    txprogress_message.progress++;
                    txprogress_message.done = false;
                    shared_memory.application_queue.push(txprogress_message);
                } else {
                    progress_count++;
                }
            }
        } else {
            re = 0;
            im = 0;

            buffer.p[i] = {re, im};
        }
    }
}

void FSKTxProcessor::on_message(const Message* const message) {
    if (message->id == Message::ID::BTLETxConfigure) {
        configure(*reinterpret_cast<const BTLETxConfigureMessage*>(message));
    }
}

void FSKTxProcessor::configure(const BTLETxConfigureMessage& message) {
    channel_number = message.channel_number;

    memcpy(advertisementData, message.advertisementData, sizeof(advertisementData));

    packets.channel_number = channel_number;

    // Calculates the samples based on the BLE packet data and generates IQ values into an array to be sent out.
    calculate_pkt_info(&packets);

    length = (uint32_t)packets.num_phy_bit;

    // Starting at sample_count 0 since packets.num_phy_sample contains every sample needed to be sent out.
    sample_count = 0;
    progress_count = 0;
    progress_notice = 64;

    txprogress_message.progress = 0;
    txprogress_message.done = false;
    configured = true;
}

int main() {
    EventDispatcher event_dispatcher{std::make_unique<FSKTxProcessor>()};
    event_dispatcher.run();
    return 0;
}
