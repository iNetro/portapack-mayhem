/*
 * Copyright (C) 2015 Jared Boone, ShareBrained Technology, Inc.
 * Copyright (C) 2016 Furrtek
 * Copyright (C) 2020 Shao
 * Copyright (C) 2023 Netro
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

 #ifndef __PROC_LRW_RX_H__
 #define __PROC_LRW_RX_H__
 
 #include "baseband_processor.hpp"
 #include "baseband_thread.hpp"
 #include "rssi_thread.hpp"
 
 #include "dsp_decimate.hpp"
 #include "dsp_demodulate.hpp"
 
 #include "audio_output.hpp"
 
 #include "fifo.hpp"
 #include "message.hpp"
 #include "../common/message_tools/src/TurboDecoder.h"

  #include <math.h>
 
 class LRWRxProcessor : public BasebandProcessor {
    public:
     void execute(const buffer_c8_t& buffer) override;
     void on_message(const Message* const message) override;
 
    private:
        static constexpr int SAMPLE_PER_SYMBOL{4};
        static constexpr uint32_t DEFAULT_PREAMBLE{0xAAAAAAAA};
        static constexpr uint32_t DEFAULT_SYNC_WORD{0x84B3E374};
        static constexpr int NUM_SYNC_WORD_BYTE{4};
        static constexpr int NUM_DATA_BYTE{360};
        static constexpr int NUM_DECODED_BYTE{200};
        static constexpr int ROLLING_WINDOW {32};
        static constexpr uint8_t MAX_BUFFERS {1};
 
     enum Parse_State {
         Parse_State_Wait_For_Peak = 0,
         Parse_State_Preamble,
         Parse_State_Sync,
         Parse_State_PDU_Payload
     };
 
     static constexpr size_t baseband_fs = 960000;
     static constexpr size_t decimated_fs = baseband_fs / 32;
 
     float detect_peak_power(const buffer_c8_t& buffer, int N);
     void agc_correct_iq(const buffer_c8_t& buffer, int N, float measured_power);
     float get_phase_diff(const complex16_t &sample0, const complex16_t &sample1);
     void demodulateFSKBits(const buffer_c16_t& decimator_out, int num_demod_byte, bool hande);
     void resetPreambleTracking();
     void resetBitPacketIndex();

     void handlePreambleState(const buffer_c16_t &decimator_out);
     void handleSyncWordState(const buffer_c16_t &decimator_out);
     void handlePDUPayloadState(const buffer_c16_t &decimator_out);
 
     std::array<complex16_t, 512> dst{};
     const buffer_c16_t dst_buffer{
         dst.data(),
         dst.size()};
 
     static constexpr int RB_SIZE = NUM_SYNC_WORD_BYTE;
     uint8_t rb_buf[RB_SIZE];

     //std::vector<float> input_bits = std::vector<float>(NUM_DATA_BYTE * 8, 0.0f);
     float input_bits[MAX_BUFFERS][NUM_DATA_BYTE * 8];
     uint8_t output_bits[MAX_BUFFERS][NUM_DECODED_BYTE * 8] = {{0}};
    
     TurboDecoder turbo_decoder{
        {
             .k = 320,
             .n = 576,
             .k_row = 18,
             .k_col = 18,
             .gk_row = 7,
             .gk_col = 7,
             .b = 0,
             .q = 4,
             .row_poly = 123,
             .col_poly = 123,
             .scale_row = 0.25f,
             .num_iter = 3,
             .m_inlen = 576,
             .m_outlen = 320,
             .m_dec_output = std::vector<float>(576, 0.0f),
             .m_dec_input = std::vector<float>(576, 0.0f),
         }
    };
 
     dsp::decimate::FIRC8xR16x24FS4Decim4 decim_0{};
     dsp::decimate::FIRC16xR16x32Decim8 decim_1{};

     dsp::demodulate::FM demod{};
     int rb_head{-1};
     int32_t g_threshold{0};
     uint8_t channel_number{0};
 
     uint16_t process = 0;
 
     bool configured{false};
     FskPacketData fskPacketData{};
 
     Parse_State parseState{Parse_State_Wait_For_Peak};

     int sample_idx{0};
     int samples_eaten{0};

     int32_t max_dB{0};
     int8_t real{0};
     int8_t imag{0};

     uint8_t peak_timeout {0};
     float noise_floor {12.0}; // Using LNA 40 and VGA 20. 10.0 was 40/0 ratio.
     float target_power_db {2.0};

     float frequency_offset_estimate {0.0f};
     float frequency_offset {0.0f};
     float phase_buffer[ROLLING_WINDOW] = {0.0f};
     int phase_buffer_index = 0;
     
     uint16_t packet_index {0};
     uint8_t bit_index {0};
     bool decode_index {0};

     /* NB: Threads should be the last members in the class definition. */
     BasebandThread baseband_thread{baseband_fs, this, baseband::Direction::Receive};
     RSSIThread rssi_thread{};
 
     void configure(const FSKRxConfigureMessage& message);
 
     /**
      * Static table used for the table_driven implementation.
      *****************************************************************************/
     const uint_fast32_t crc_table[256] {{0}};
     // clang-format on
 };
 
 #endif /*__PROC_LRW_RX_H__*/
 