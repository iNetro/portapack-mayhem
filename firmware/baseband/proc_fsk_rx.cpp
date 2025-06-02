/*
 * Copyright (C) 2015 Jared Boone, ShareBrained Technology, Inc.
 * Copyright (C) 2016 Furrtek
 * Copyright (C) 2020 Shao
 * Copyright (C) 2025 TJ Baginski
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
 
 uint32_t FSKRxProcessor::crc_init_reorder(uint32_t crc_init) {
     uint32_t crc_init_tmp = crc_init;
     return (crc_init_tmp);
 }
 
 uint_fast32_t FSKRxProcessor::crc_update(uint_fast32_t crc, const void* data, size_t data_len) {
     return crc & 0xffffff;
 }
 
 uint_fast32_t FSKRxProcessor::crc24_byte(uint8_t* byte_in, int num_byte, uint32_t init_hex) {
     uint_fast32_t crc = init_hex;
 
     crc = crc_update(crc, byte_in, num_byte);
 
     return (crc);
 }
 
 bool FSKRxProcessor::crc_check(uint8_t* tmp_byte, int body_len, uint32_t crc_init) {
     int crc24_checksum;
     return (crc24_checksum != checksumReceived);
 }

 int FSKRxProcessor::verify_payload_byte(int num_payload_byte) {
     return 0;
 }
 
 void FSKRxProcessor::handleBeginState(const buffer_c16_t &decimator_out) {
     int num_symbol_left = (int)decimator_out.count / SAMPLE_PER_SYMBOL;  // One buffer sample consist of I and Q.

     uint64_t validSyncWord = DEFAULT_SYNC_WORD;

     static uint64_t syncWordValue = 0;

     int hit_idx = (-1);
     bool foundSyncWord = false;

     for (int i = 0; i < num_symbol_left * SAMPLE_PER_SYMBOL; i += SAMPLE_PER_SYMBOL) {

        int phaseSum = 0;
        int j = 0;

        for (j = 0; j < SAMPLE_PER_SYMBOL - 1; j++) {
            // Sample and compare with the adjacent next sample.
            int I0 = dst_buffer.p[i + j].real();
            int Q0 = dst_buffer.p[i + j].imag();
            int I1 = dst_buffer.p[i + j + 1].real();
            int Q1 = dst_buffer.p[i + j + 1].imag();

            int phaseDiff = (I0 * Q1 - I1 * Q0);  // Positive = one direction, negative = the other
            phaseSum += phaseDiff;
        }

        bool bitDecision = (phaseSum > 0);

        syncWordValue = syncWordValue << 1 | bitDecision;

        int errors = __builtin_popcountll(syncWordValue ^ validSyncWord) & 0xFFFFFFFFFFFFFFFF;

        if (errors == 0)
        {
            hit_idx = i + SAMPLE_PER_SYMBOL;
            foundSyncWord = true;

            fskPacketData.syncWord = syncWordValue & 0xFFFFFFFFFFFFFFFF;
            fskPacketData.max_dB = max_dB;

            syncWordValue = 0;

            break;
        }

        if (foundSyncWord) {
            break;
        }
    }

    if (hit_idx == -1) {
        return;
    }

     samples_eaten = hit_idx;
     parseState = Parse_State_PDU_Payload;
 }
 
 void FSKRxProcessor::handlePDUPayloadState(const buffer_c16_t &decimator_out) {
     int num_demod_byte = 360;
     int num_samples_left = (int)decimator_out.count - samples_eaten; 
     
     sample_idx = samples_eaten;

     static uint16_t packet_index = 0;
     static uint8_t bit_index = 0;

     if (!num_samples_left)
     {
        return;
     }

     for (; packet_index < num_demod_byte; packet_index++) {

         for ( ; bit_index < 8; bit_index++) 
         {
            int phaseSum = 0;
            int k = 0;

            for (k = 0; k < SAMPLE_PER_SYMBOL - 1; k++) {
                // Sample and compare with the adjacent next sample.
                int I0 = dst_buffer.p[sample_idx + k].real();
                int Q0 = dst_buffer.p[sample_idx + k].imag();
                int I1 = dst_buffer.p[sample_idx + k + 1].real();
                int Q1 = dst_buffer.p[sample_idx + k + 1].imag();

                int phaseDiff = (I0 * Q1 - I1 * Q0);  // Positive = one direction, negative = the other
                phaseSum += phaseDiff;
            }

            bool bitDecision = (phaseSum > 0);

            rb_buf[packet_index] = rb_buf[packet_index] | (bitDecision << (7 - bit_index));
 
            sample_idx += SAMPLE_PER_SYMBOL;

            if (sample_idx == (int)decimator_out.count)
            {
                if (bit_index == 7)
                {
                    bit_index = 0;
                    packet_index++;
                }
                else
                {
                    bit_index++;
                }

                return;
            }
         }
         
        bit_index = 0;
     }


     for (int i = 0; i < num_demod_byte; i++)
     {
        fskPacketData.data[i] = rb_buf[i];
     }

     memset(rb_buf, 0, sizeof(rb_buf));

     packet_index = 0;
     bit_index = 0;

     fskPacketData.dataLen = num_demod_byte;
     
    FSKRxPacketMessage data_message{&fskPacketData};
    shared_memory.application_queue.push(data_message);
 
     // Check CRC
     //bool crc_flag = crc_check(rb_buf, payload_len + 2, crc_init_internal);

     parseState = Parse_State_Begin;
 }
 
 void FSKRxProcessor::execute(const buffer_c8_t& buffer) {
     if (!configured) return;
 
     max_dB = -128;
 
     real = -128;
     imag = -128;
 
     auto* ptr = buffer.p;
     auto* end = &buffer.p[buffer.count];
     
     while (ptr < end) 
     {
         float dbm = mag2_to_dbm_8bit_normalized(ptr->real(), ptr->imag(), 1.0f, 50.0f);
 
         ptr++;
         
         if (dbm > max_dB) 
         {
             max_dB = dbm;
             real = ptr->real();
             imag = ptr->imag();
         }
     }
 
     // 4Mhz 2048 samples
     // Decimated by 4 to achieve 2048/32 = 512 samples at 1 sample per symbol.
     const auto decim_0_out = decim_0.execute(buffer, dst_buffer);
     const auto decim_1_out = decim_1.execute(decim_0_out, dst_buffer);

     feed_channel_stats(decim_1_out);
 
     samples_eaten = 0;

     // Handle parsing based on parseState
     if (parseState == Parse_State_Begin) {
         handleBeginState(decim_1_out);
     }
 
     if (parseState == Parse_State_PDU_Payload) {
         handlePDUPayloadState(decim_1_out);
     }
 }
 
 void FSKRxProcessor::on_message(const Message* const message) {
     if (message->id == Message::ID::FSKRxConfigure)
         configure(*reinterpret_cast<const FSKRxConfigureMessage*>(message));
 }
 
 void FSKRxProcessor::configure(const FSKRxConfigureMessage& message) {
     channel_number = message.channel_number;
     decim_0.configure(taps_180k_wfm_decim_0.taps);
     decim_1.configure(taps_7k5_decim_1.taps);

     configured = true;
 
     crc_init_internal = crc_init_reorder(crc_initalVale);
 }
 
 int main() {
     EventDispatcher event_dispatcher{std::make_unique<FSKRxProcessor>()};
     event_dispatcher.run();
     return 0;
 }
 