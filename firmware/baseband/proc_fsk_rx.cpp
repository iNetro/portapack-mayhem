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
 #include "sine_table_int8.hpp"
 
 #include "event_m4.hpp"

float FSKRxProcessor::detect_peak_power(const buffer_c8_t& buffer, int N) 
{
    int32_t power = 0;

    // Initial window power
    for (int i = 0; i < N; i++) {
        int16_t i_sample = buffer.p[i].real();
        int16_t q_sample = buffer.p[i].imag();
        power += i_sample * i_sample + q_sample * q_sample;
    }

    power = power / N;

    // Convert to dB over noise floor
    float power_db = 10.0f * log10f((float)power / noise_floor);

    // If too weak, treat as no signal
    if (power_db <= 0.0f) return 0;

    return power_db;
}

void FSKRxProcessor::agc_correct_iq(const buffer_c8_t& buffer, int N, float measured_power) 
{
    float power_db = 10.0f * log10f(measured_power / noise_floor);
    float error_db = target_power_db - power_db;

    if (!error_db)
    {
        return;
    }

    float gain_scalar = powf(10.0f, error_db / 20.0f);

    for (int i = 0; i < N; i++)
    {
        buffer.p[i] = {buffer.p[i].real() * gain_scalar, buffer.p[i].imag() * gain_scalar};
    }
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

    parseState = Parse_State_Wait_For_Peak;
 }
 
 void FSKRxProcessor::execute(const buffer_c8_t& buffer) 
 {
    if (!configured) return;

    float power = detect_peak_power(buffer, buffer.count);

    if (power > 1.0f)
    {
        agc_correct_iq(buffer, buffer.count, power);

        if (parseState == Parse_State_Wait_For_Peak)
        {
            parseState = Parse_State_Begin;
            fskPacketData.power = power;
            peak_timeout = 0;
        }
    }
    else
    {
        peak_timeout++;

        if (peak_timeout == 255)
        {
            parseState = Parse_State_Wait_For_Peak;
            peak_timeout = 0;
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
     decim_0.configure(taps_50k0_lrw_decim_0.taps);
     decim_1.configure(taps_8k0_lrw_decim_1.taps);

     configured = true;
 }
 
 int main() {
     EventDispatcher event_dispatcher{std::make_unique<FSKRxProcessor>()};
     event_dispatcher.run();
     return 0;
 }
 