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

 #ifndef __PROC_FSK_RX_H__
 #define __PROC_FSK_RX_H__
 
 #include "baseband_processor.hpp"
 #include "baseband_thread.hpp"
 #include "rssi_thread.hpp"
 
 #include "dsp_decimate.hpp"
 #include "dsp_demodulate.hpp"
 
 #include "audio_output.hpp"
 
 #include "fifo.hpp"
 #include "message.hpp"
 
 class FSKRxProcessor : public BasebandProcessor {
    public:
     void execute(const buffer_c8_t& buffer) override;
     void on_message(const Message* const message) override;
 
    private:
        static constexpr int SAMPLE_PER_SYMBOL{1};
        static constexpr int LEN_DEMOD_BUF_SYNC_WORD{32};
        static constexpr uint32_t DEFAULT_SYNC_WORD{0x84B3E374};
        static constexpr int NUM_SYNC_WORD_BYTE{4};
 
     enum Parse_State {
         Parse_State_Begin = 0,
         Parse_State_PDU_Header,
         Parse_State_PDU_Payload
     };
 
     uint8_t macAddress[6];
     int checksumReceived = 0;

     static constexpr size_t baseband_fs = 30000;
 
     uint_fast32_t crc_update(uint_fast32_t crc, const void* data, size_t data_len);
     uint_fast32_t crc24_byte(uint8_t* byte_in, int num_byte, uint32_t init_hex);
     bool crc_check(uint8_t* tmp_byte, int body_len, uint32_t crc_init);
     uint32_t crc_init_reorder(uint32_t crc_init);
 
     uint32_t crc_initalVale = 0x555555;
     uint32_t crc_init_internal = 0x00;
 
     void scramble_byte(uint8_t* byte_in, int num_byte, const uint8_t* scramble_table_byte, uint8_t* byte_out);
     // void demod_byte(int num_byte, uint8_t *out_byte);
     int verify_payload_byte(int num_payload_byte);
 
     void handleBeginState(const buffer_c16_t &decimator_out);
     void handlePDUHeaderState(const buffer_c16_t &decimator_out);
     void handlePDUPayloadState(const buffer_c16_t &decimator_out);
 
     std::array<complex16_t, 512> dst{};
     const buffer_c16_t dst_buffer{
         dst.data(),
         dst.size()};
 
     static constexpr int RB_SIZE = 512;
     uint8_t rb_buf[RB_SIZE];
 
     dsp::decimate::FIRC8xR16x24FS4Decim4 decim_0{};
     dsp::decimate::FIRC16xR16x32Decim8 decim_1{};

     dsp::demodulate::FM demod{};
     int rb_head{-1};
     int32_t g_threshold{0};
     uint8_t channel_number{37};
 
     uint16_t process = 0;
 
     bool configured{false};
     FskPacketData fskPacketData{};
 
     Parse_State parseState{Parse_State_Begin};
     uint16_t packet_index{0};
     int sample_idx{0};
     int symbols_eaten{0};
     uint8_t bit_decision{0};
     uint8_t payload_len{0};
     uint8_t pdu_type{0};
     int32_t max_dB{0};
     int8_t real{0};
     int8_t imag{0};
 
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
 
 #endif /*__PROC_FSK_RX_H__*/
 