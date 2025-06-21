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
 #include "message.hpp"
 
 #include "event_m4.hpp"

 #ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

    if (error_db <= 0)
    {
        return;
    }

    float gain_scalar = powf(10.0f, error_db / 20.0f);

    for (int i = 0; i < N; i++)
    {
        buffer.p[i] = {buffer.p[i].real() * gain_scalar, buffer.p[i].imag() * gain_scalar};
    }
}

float FSKRxProcessor::get_phase_diff(const complex16_t &sample0, const complex16_t &sample1)
{
    // Calculate the phase difference between two samples.
    float dI = sample1.real() * sample0.real() + sample1.imag() * sample0.imag();
    float dQ = sample1.imag() * sample0.real() - sample1.real() * sample0.imag();
    float phase_diff = atan2f(dQ, dI);

    return phase_diff;
}

void FSKRxProcessor::demodulateFSKBits(const buffer_c16_t& decimator_out, int num_demod_byte) 
{
    for (; packet_index < num_demod_byte; packet_index++) 
    {
        for (; bit_index < 8; bit_index++) 
        {
            if (samples_eaten >= (int)decimator_out.count) 
            {
                return;
            }

            float phaseSum = 0.0f;
            for (int k = 0; k < SAMPLE_PER_SYMBOL - 1; ++k) 
            {
                float phase = get_phase_diff(
                    decimator_out.p[samples_eaten + k],
                    decimator_out.p[samples_eaten + k + 1]
                );
                phaseSum += phase;
            }

            phaseSum /= (SAMPLE_PER_SYMBOL - 1);
            phaseSum -= frequency_offset;

            bool bitDecision = (phaseSum > 0.0f);
            rb_buf[packet_index] |= (bitDecision << (7 - bit_index));

            input_bits[packet_index * 8 + bit_index] = phaseSum;

            samples_eaten += SAMPLE_PER_SYMBOL;
        }

        bit_index = 0;
    }
}

void FSKRxProcessor::resetPreambleTracking() 
{
    frequency_offset = 0.0f;
    frequency_offset_estimate = 0.0f;
    phase_buffer_index = 0;
    memset(phase_buffer, 0, sizeof(phase_buffer));
}

void FSKRxProcessor::resetBitPacketIndex() 
{
    packet_index = 0;
    bit_index = 0;
}

void FSKRxProcessor::handlePreambleState(const buffer_c16_t &decimator_out) 
{
    int num_symbols = (int)decimator_out.count / SAMPLE_PER_SYMBOL;
    const uint32_t validPreamble = DEFAULT_PREAMBLE;
    static uint32_t preambleValue = 0;

    int hit_idx = -1;

    for (int i = 0; i < num_symbols * SAMPLE_PER_SYMBOL; i += SAMPLE_PER_SYMBOL) 
    {
        float phaseSum = 0.0f;

        for (int j = 0; j < SAMPLE_PER_SYMBOL - 1; j++) 
        {
            phaseSum += get_phase_diff(decimator_out.p[i + j], decimator_out.p[i + j + 1]);
        }

        phase_buffer[phase_buffer_index] = phaseSum / (SAMPLE_PER_SYMBOL - 1);
        phase_buffer_index = (phase_buffer_index + 1) % ROLLING_WINDOW;

        bool bitDecision = (phaseSum > 0.0f);
        preambleValue = (preambleValue << 1) | bitDecision;

        int errors = __builtin_popcountl(preambleValue ^ validPreamble) & 0xFFFFFFFF;

        if (errors == 0) 
        {
            hit_idx = i + SAMPLE_PER_SYMBOL;
            fskPacketData.syncWord = preambleValue;
            fskPacketData.max_dB = max_dB;

            for (int k = 0; k < ROLLING_WINDOW; k++) 
            {
                frequency_offset_estimate += phase_buffer[k];
            }

            frequency_offset = frequency_offset_estimate / ROLLING_WINDOW;

            fskPacketData.frequency_offset_hz = (frequency_offset * decimated_fs) / (2.0f * M_PI);

            resetPreambleTracking();

            preambleValue = 0;
            break;
        }
    }

    if (hit_idx == -1) return;

    samples_eaten = hit_idx;
    parseState = Parse_State_Sync;
}

void FSKRxProcessor::handleSyncWordState(const buffer_c16_t &decimator_out) {
    const int syncword_bytes = 4;
    const uint32_t validSyncWord = DEFAULT_SYNC_WORD;

    if ((int)decimator_out.count - samples_eaten <= 0) 
    {
        return;
    }

    demodulateFSKBits(decimator_out, syncword_bytes);

    if (packet_index < syncword_bytes || bit_index != 0)
    {
        return;
    } 

    uint32_t receivedSyncWord = (rb_buf[0] << 24) | (rb_buf[1] << 16) | (rb_buf[2] << 8)  | rb_buf[3];

    int errors = __builtin_popcountl(receivedSyncWord ^ validSyncWord) & 0xFFFFFFFF;

    if (errors <=2) 
    {
        fskPacketData.syncWord = receivedSyncWord;
        parseState = Parse_State_PDU_Payload;
        memset(fskPacketData.data, 0, sizeof(fskPacketData.data));
    } 
    else 
    {
        parseState = Parse_State_Wait_For_Peak;
    }

    memset(rb_buf, 0, sizeof(rb_buf));
    resetBitPacketIndex();
}
 
void FSKRxProcessor::handlePDUPayloadState(const buffer_c16_t &decimator_out) 
{
    if ((int)decimator_out.count - samples_eaten <= 0) 
    {
        return;
    }

    demodulateFSKBits(decimator_out, NUM_DATA_BYTE);

    if (packet_index < NUM_DATA_BYTE || bit_index != 0) 
    {
        return;
    }

    LRWDecodeMessage decode_message;
    shared_memory.application_queue.push(decode_message);

    memset(rb_buf, 0, sizeof(rb_buf));
    resetBitPacketIndex();

    parseState = Parse_State_Wait_For_Peak;
}
 
 void FSKRxProcessor::execute(const buffer_c8_t& buffer) 
 {
    if (!configured) return;

    float power = detect_peak_power(buffer, buffer.count);

    if (power)
    {
        agc_correct_iq(buffer, buffer.count, power);

        if (parseState == Parse_State_Wait_For_Peak)
        {
            parseState = Parse_State_Preamble;
            fskPacketData.power = power;
            peak_timeout = 0;
            resetPreambleTracking();
        }
    }
    else
    {
        if (parseState != Parse_State_Wait_For_Peak)
        {
            peak_timeout++;

            // 960,000 fs / 2048 samples = 468.75 Hz, so 55 calls is about 0.053 seconds before timeout.
            if (peak_timeout == 30) 
            {
                parseState = Parse_State_Wait_For_Peak;
                peak_timeout = 0;
            }
        }
    }
     
    // 4Mhz 2048 samples
    // Decimated by 4 to achieve 2048/32 = 512 samples at 1 sample per symbol.
    const auto decim_0_out = decim_0.execute(buffer, dst_buffer);
    const auto decim_1_out = decim_1.execute(decim_0_out, dst_buffer);

    feed_channel_stats(decim_1_out);

    samples_eaten = 0;

    if (parseState == Parse_State_Preamble) {
        handlePreambleState(decim_1_out);
    }

    if (parseState == Parse_State_Sync) {
        handleSyncWordState(decim_1_out);
    }

    if (parseState == Parse_State_PDU_Payload) {
        handlePDUPayloadState(decim_1_out);
    }
 }
 
 void FSKRxProcessor::on_message(const Message* const message) {
    if (message->id == Message::ID::FSKRxConfigure)
    {
        configure(*reinterpret_cast<const FSKRxConfigureMessage*>(message));
    }
    else if (message->id == Message::ID::LRWDecodedPacket) 
    {
        fskPacketData.dataLen = NUM_DECODED_BYTE;

        turbo_decoder.decode(input_bits, output_bits, 5);
        turbo_decoder.lfsr_dewhiten(output_bits);

        // Copy the decoded bits to the packet data
        for (int i = 0; i < NUM_DECODED_BYTE; i++) 
        {
            for (int j = 0; j < 8; j++) 
            {
                fskPacketData.data[i] |= (output_bits[i * 8 + j] << (7 - j));
            }
        }

        FSKRxPacketMessage data_message{&fskPacketData};
        shared_memory.application_queue.push(data_message);

        memset(rb_buf, 0, sizeof(rb_buf));
        memset(output_bits.data(), 0, output_bits.size() * sizeof(uint8_t));
        memset(input_bits.data(), 0, input_bits.size() * sizeof(float));
    }
 }
 
 void FSKRxProcessor::configure(const FSKRxConfigureMessage& message) {
     channel_number = message.channel_number;
     decim_0.configure(taps_60k0_lrw_decim_0.taps);
     decim_1.configure(taps_13k0_lrw_decim_1.taps);

     configured = true;
 }
 
 int main() {
     EventDispatcher event_dispatcher{std::make_unique<FSKRxProcessor>()};
     event_dispatcher.run();
     return 0;
 }
 