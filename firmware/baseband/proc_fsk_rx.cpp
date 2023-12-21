/*
 * Copyright (C) 2015 Jared Boone, ShareBrained Technology, Inc.
 * Copyright (C) 2016 Furrtek
 * Copyright (C) 2020 Shao
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

#include "proc_fsk_rx.hpp"
#include "portapack_shared_memory.hpp"

#include "event_m4.hpp"

uint8_t FSKRxProcessor::getBitAtIndex(uint32_t value, int index) {
    if (index >= 0 && index < 32) {
        return (value >> index) & 0x01;
    } else {
        return 0;
    }
}

bool FSKRxProcessor::checkSync(uint32_t findSync, uint8_t* demod_sync_byte, int startIndex) {
    bool unequal_flag = false;

    for (int p = 0; p < LEN_DEMOD_BUF_ACCESS; p++) {
        if (demod_sync_byte[startIndex] != getBitAtIndex(findSync, p)) {
            unequal_flag = true;
            break;
        }
        
        syncByte = (syncByte & (~(1U << p))) | (demod_sync_byte[startIndex] << p);
        
        startIndex = ((startIndex + 1) & (LEN_DEMOD_BUF_ACCESS - 1));
    }

    return unequal_flag;
}

void FSKRxProcessor::handleBeginState() {
    int num_symbol_left = dst_buffer.count / SAMPLE_PER_SYMBOL;  // One buffer sample consist of I and Q.

    static uint8_t demod_buf_access[SAMPLE_PER_SYMBOL][LEN_DEMOD_BUF_ACCESS];

    const int demod_buf_len = LEN_DEMOD_BUF_ACCESS;  // For AA
    int demod_buf_offset = 0;
    int hit_idx = (-1);
    bool unequal_flag = false;

    memset(demod_buf_access, 0, SAMPLE_PER_SYMBOL * demod_buf_len);

    for (int i = 0; i < num_symbol_left * SAMPLE_PER_SYMBOL; i += SAMPLE_PER_SYMBOL) {
        int sp = ((demod_buf_offset - demod_buf_len + 1) & (demod_buf_len - 1));

        for (int j = 0; j < SAMPLE_PER_SYMBOL; j++) {
            // Sample and compare with the adjacent next sample.
            int I0 = dst_buffer.p[i + j].real();
            int Q0 = dst_buffer.p[i + j].imag();
            int I1 = dst_buffer.p[i + j + 1].real();
            int Q1 = dst_buffer.p[i + j + 1].imag();

            phase_idx = j;

            demod_buf_access[phase_idx][demod_buf_offset] = (I0 * Q1 - I1 * Q0) > 0 ? 1 : 0;

            syncByte = 0;
            unequal_flag = checkSync(SYNC_BYTE_BLE, demod_buf_access[phase_idx], sp);

            if (unequal_flag == false) {
                hit_idx = (i + j - (demod_buf_len - 1) * SAMPLE_PER_SYMBOL);
                break;
            }
        }

        if (unequal_flag == false) {
            break;
        }

        demod_buf_offset = ((demod_buf_offset + 1) & (demod_buf_len - 1));
    }

    if (hit_idx == -1) {
        // Process more samples.
        return;
    }

    symbols_eaten += hit_idx;

    symbols_eaten += (8 * NUM_ACCESS_ADDR_BYTE * SAMPLE_PER_SYMBOL);  // move to the beginning of PDU header

    num_symbol_left = num_symbol_left - symbols_eaten;

//----------------------------------------
    fskPacketData.max_dB = max_dB;

    fskPacketData.type = 0;
    fskPacketData.size = 31;

    fskPacketData.deviceId[0] = (syncByte >> 24) & 0xFF;
    fskPacketData.deviceId[1] = (syncByte >> 16) & 0xFF;
    fskPacketData.deviceId[2] = (syncByte >> 8) & 0xFF;
    fskPacketData.deviceId[3] = syncByte & 0xFF;
    fskPacketData.deviceId[4] = 0x01;
    fskPacketData.deviceId[5] = 0x02;

    int i;

    for (i = 0; i < 20; i++) {
        fskPacketData.data[i] = 0x01;
    }

    fskPacketData.dataLen = i;

    FskPacketMessage data_message{&fskPacketData};

    if (syncByte > 0xFF)
    {
        shared_memory.application_queue.push(data_message);
    }

//----------------------------------------

    parseState = Parse_State_Begin;
    

   // parseState = Parse_State_PDU_Header;
}

void FSKRxProcessor::handlePDUHeaderState() {
    int num_demod_byte = 2;  // PDU header has 2 octets

    symbols_eaten += 8 * num_demod_byte * SAMPLE_PER_SYMBOL;

    if (symbols_eaten > (int)dst_buffer.count) {
        return;
    }

    // Jump back down to the beginning of PDU header.
    sample_idx = symbols_eaten - (8 * num_demod_byte * SAMPLE_PER_SYMBOL);

    packet_index = 0;

    for (int i = 0; i < num_demod_byte; i++) {
        rb_buf[packet_index] = 0;

        for (int j = 0; j < 8; j++) {
            int I0 = dst_buffer.p[sample_idx].real();
            int Q0 = dst_buffer.p[sample_idx].imag();
            int I1 = dst_buffer.p[sample_idx + 1].real();
            int Q1 = dst_buffer.p[sample_idx + 1].imag();

            bit_decision = (I0 * Q1 - I1 * Q0) > 0 ? 1 : 0;
            rb_buf[packet_index] = rb_buf[packet_index] | (bit_decision << j);

            sample_idx += SAMPLE_PER_SYMBOL;
        }

        packet_index++;
    }

    parseState = Parse_State_PDU_Payload;
}

void FSKRxProcessor::handlePDUPayloadState() {
    int i;
    int num_demod_byte = (payload_len + 3);
    symbols_eaten += 8 * num_demod_byte * SAMPLE_PER_SYMBOL;

    if (symbols_eaten > (int)dst_buffer.count) {
        return;
    }

    for (i = 0; i < num_demod_byte; i++) {
        rb_buf[packet_index] = 0;

        for (int j = 0; j < 8; j++) {
            int I0 = dst_buffer.p[sample_idx].real();
            int Q0 = dst_buffer.p[sample_idx].imag();
            int I1 = dst_buffer.p[sample_idx + 1].real();
            int Q1 = dst_buffer.p[sample_idx + 1].imag();

            bit_decision = (I0 * Q1 - I1 * Q0) > 0 ? 1 : 0;
            rb_buf[packet_index] = rb_buf[packet_index] | (bit_decision << j);

            sample_idx += SAMPLE_PER_SYMBOL;
        }

        packet_index++;
    }

    // This should be the flag that determines if the data should be sent to the application layer.
    bool sendPacket = true;

    if (sendPacket) {
        fskPacketData.max_dB = max_dB;

        fskPacketData.type = pdu_type;
        fskPacketData.size = payload_len;

        fskPacketData.deviceId[0] = rb_buf[7];
        fskPacketData.deviceId[1] = rb_buf[6];
        fskPacketData.deviceId[2] = rb_buf[5];
        fskPacketData.deviceId[3] = rb_buf[4];
        fskPacketData.deviceId[4] = rb_buf[3];
        fskPacketData.deviceId[5] = rb_buf[2];

        // Skip Header Byte and MAC Address
        uint8_t startIndex = 8;

        for (i = 0; i < payload_len - 6; i++) {
            fskPacketData.data[i] = rb_buf[startIndex++];
        }

        fskPacketData.dataLen = i;

        FskPacketMessage data_message{&fskPacketData};

        shared_memory.application_queue.push(data_message);
    }

    parseState = Parse_State_Begin;
}

void FSKRxProcessor::execute(const buffer_c8_t& buffer) {
    if (!configured) return;

    // Pulled this implementation from channel_stats_collector.c to time slice a specific packet's dB.
    uint32_t max_squared = 0;

    void* src_p = buffer.p;

    while (src_p < &buffer.p[buffer.count]) {
        const uint32_t sample = *__SIMD32(src_p)++;
        const uint32_t mag_sq = __SMUAD(sample, sample);
        if (mag_sq > max_squared) {
            max_squared = mag_sq;
        }
    }

    const float max_squared_f = max_squared;
    max_dB = mag2_to_dbv_norm(max_squared_f * (1.0f / (32768.0f * 32768.0f)));

    // 4Mhz 2048 samples
    // Decimated by 4 to achieve 2048/4 = 512 samples at 1 sample per symbol.
    decim_0.execute(buffer, dst_buffer);
    feed_channel_stats(dst_buffer);

    symbols_eaten = 0;

    // Handle parsing based on parseState
    if (parseState == Parse_State_Begin) {
        handleBeginState();
    }

    if (parseState == Parse_State_PDU_Header) {
        handlePDUHeaderState();
    }

    if (parseState == Parse_State_PDU_Payload) {
        handlePDUPayloadState();
    }
}

void FSKRxProcessor::on_message(const Message* const message) {

    switch (message->id) {
        case Message::ID::FSKRxConfigure:
            configure(*reinterpret_cast<const FSKRxConfigureMessage*>(message));
            break;
        default:
            break;
    }
}

void FSKRxProcessor::configure(const FSKRxConfigureMessage& message) {
    channel_number = message.deviation;
    decim_0.configure(taps_BTLE_1M_PHY_decim_0.taps);

    configured = true;
}

int main() {
    EventDispatcher event_dispatcher{std::make_unique<FSKRxProcessor>()};
    event_dispatcher.run();
    return 0;
}
