/*
 * Copyright (C) 2014 Jared Boone, ShareBrained Technology, Inc.
 * Copyright (C) 2017 Furrtek
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

#ifndef __LRW_TX_APP_H__
#define __LRW_TX_APP_H__

#include "ui.hpp"
#include "ui_navigation.hpp"
#include "ui_receiver.hpp"
#include "ui_transmitter.hpp"
#include "ui_text_editor.hpp"
#include "ui_freq_field.hpp"
#include "ui_record_view.hpp"
#include "app_settings.hpp"
#include "radio_state.hpp"
#include "replay_thread.hpp"
#include "log_file.hpp"
#include "utility.hpp"
#include "file_path.hpp"

#include "recent_entries.hpp"

#include <string>
#include <memory>

namespace ui {

struct LRWTxPacket {
    char advertisementData[65];
    char packetCount[11];
    char deviceId[11];
    uint32_t packet_count;
};

class LRWTxView : public View {
   public:
    LRWTxView(NavigationView& nav);
    LRWTxView(NavigationView& nav, LRWTxPacket packet);
    ~LRWTxView();

    void set_parent_rect(const Rect new_parent_rect) override;
    void paint(Painter&) override{};

    void focus() override;

    bool is_active() const;
    void toggle();
    void start();
    void stop();
    void reset();
    void handle_replay_thread_done(const uint32_t return_code);
    void file_error();
    bool saveFile(const std::filesystem::path& path);

    std::uint64_t get_freq_by_channel_number_fsk(uint8_t channel_number);
    std::string title() const override { return "LRW TX"; };

   private:
    void on_timer();
    void on_file_changed(const std::filesystem::path& new_file_path);
    void on_save_file(const std::string value);
    void on_tx_progress(const bool done);
    void update_current_packet(LRWTxPacket packet, uint32_t currentIndex);
    std::vector<std::string> splitIntoStrings(const char* input);
    uint32_t stringToUint32(const std::string& str);
    bool hasValidHexPairs(const std::string& str, int totalPairs);

    NavigationView& nav_;
    TxRadioState radio_state_{
        902'075'000 /* frequency */,
        30000 /* bandwidth */,
        30000 /* sampling rate */
    };
    app_settings::SettingsManager settings_{
        "tx_lrw", app_settings::Mode::TX};

    uint8_t console_color{0};
    uint32_t prev_value{0};

    std::filesystem::path file_path{};
    std::filesystem::path packet_save_path{bletx_dir / u"LRWTX_????.TXT"};
    uint8_t channel_number = 0;
    bool auto_channel = false;

    char randomMac[11] = "0102030405";

    bool is_running = false;

    int16_t timer_count{0};
    int16_t timer_period{1};
    int16_t auto_channel_counter = 0;
    int16_t auto_channel_period{6};

    bool repeatLoop = false;
    uint32_t packet_counter{0};
    uint32_t num_packets{0};
    uint32_t current_packet{0};
    bool random_mac = false;
    bool file_override = false;

    typedef struct {
        uint16_t line;
        uint16_t col;
    } CursorPos;

    std::unique_ptr<FileWrapper> dataFileWrapper{};
    File dataFile{};
    std::filesystem::path dataTempFilePath{bletx_dir / u"dataFileTemp.TXT"};
    std::vector<uint16_t> markedBytes{};
    CursorPos cursor_pos{};
    uint8_t marked_counter = 0;

    static constexpr uint8_t device_id_size_str{10};
    static constexpr uint16_t max_packet_size_str{64};
    static constexpr uint8_t max_packet_repeat_str{10};
    static constexpr uint32_t max_packet_repeat_count{UINT32_MAX};
    static constexpr uint32_t max_num_packets{1};

    LRWTxPacket packets[max_num_packets];

    static constexpr auto header_height = 10 * 16;
    static constexpr auto switch_button_height = 6 * 16;

    Button button_open{
        {0 * 8, 0 * 16, 10 * 8, 2 * 16},
        "Open file"};

    Text text_filename{
        {11 * 8, 0 * 16, 12 * 8, 16},
        "-"};

    ProgressBar progressbar{
        {11 * 8, 1 * 16, 9 * 8, 16}};

    Checkbox check_rand_mac{
        {21 * 8, 1 * 16},
        6,
        "?? ID",
        true};

    TxFrequencyField field_frequency{
        {0 * 8, 2 * 16},
        nav_};

    TransmitterView2 tx_view{
        {11 * 8, 2 * 16},
        /*short_ui*/ true};

    Checkbox check_loop{
        {21 * 8, 2 * 16},
        4,
        "Loop",
        true};

    ImageButton button_play{
        {28 * 8, 2 * 16, 2 * 8, 1 * 16},
        &bitmap_play,
        Theme::getInstance()->fg_green->foreground,
        Theme::getInstance()->fg_green->background};

    Labels label_speed{
        {{0 * 8, 6 * 8}, "Speed:", Theme::getInstance()->fg_light->foreground}};

    OptionsField options_speed{
        {7 * 8, 6 * 8},
        3,
        {{"1 ", 1},     // 16ms
         {"2 ", 2},     // 32ms
         {"3 ", 3},     // 48ms
         {"4 ", 6},     // 100ms
         {"5 ", 12}}};  // 200ms

    OptionsField options_channel{
        {11 * 8, 6 * 8},
        5,
        {{"Ch.00", 0},
         {"Ch.01", 1},
         {"Ch.02", 2},
         {"Ch.03", 3},
         {"Ch.04", 4},
         {"Ch.05", 5},
         {"Ch.06", 6},
         {"Ch.07", 7},
         {"Ch.08", 8},
         {"Ch.09", 9},
         {"Ch.10", 10},
         {"Ch.11", 11},
         {"Ch.12", 12},
         {"Ch.13", 13},
         {"Ch.14", 14},
         {"Ch.15", 15},
         {"Auto", 40}}};

    OptionsField options_adv_type{
        {17 * 8, 6 * 8},
        14,
        {{"NA ", 0}}};

    Labels label_marked_data{
        {{0 * 8, 4 * 16}, "Marked Data:", Theme::getInstance()->fg_light->foreground}};

    OptionsField marked_data_sequence{
        {12 * 8, 8 * 8},
        8,
        {{"Ascend", 0},
         {"Descend", 1},
         {"Random", 2}}};

    Labels label_packet_index{
        {{0 * 8, 12 * 8}, "Packet Index:", Theme::getInstance()->fg_light->foreground}};

    Text text_packet_index{
        {13 * 8, 6 * 16, 12 * 8, 16},
        "-"};

    Labels label_packets_sent{
        {{0 * 8, 14 * 8}, "Repeat Count:", Theme::getInstance()->fg_light->foreground}};

    Text text_packets_sent{
        {13 * 8, 7 * 16, 12 * 8, 16},
        "-"};

    Labels label_device_id{
        {{0 * 8, 16 * 8}, "Device ID:", Theme::getInstance()->fg_light->foreground}};

    Text text_device_id{
        {12 * 8, 8 * 16, 20 * 8, 16},
        "-"};

    Labels label_data_packet{
        {{0 * 8, 9 * 16}, "Packet Data:", Theme::getInstance()->fg_light->foreground}};

    TextViewer dataEditView{
        {0, 9 * 18, 240, 240}};

    Button button_clear_marked{
        {1 * 8, 14 * 16, 13 * 8, 3 * 8},
        "Clear Marked"};

    Button button_save_packet{
        {1 * 8, 16 * 16, 13 * 8, 2 * 16},
        "Save Packet"};

    Button button_switch{
        {16 * 8, 16 * 16, 13 * 8, 2 * 16},
        "Switch to Rx"};

    std::string str_log{""};
    bool logging{true};
    bool logging_done{false};
    std::string packetFileBuffer{};

    MessageHandlerRegistration message_handler_tx_progress{
        Message::ID::TXProgress,
        [this](const Message* const p) {
            const auto message = *reinterpret_cast<const TXProgressMessage*>(p);
            this->on_tx_progress(message.done);
        }};

    MessageHandlerRegistration message_handler_frame_sync{
        Message::ID::DisplayFrameSync,
        [this](const Message* const) {
            this->on_timer();
        }};
};

} /* namespace ui */

#endif /*__UI_AFSK_RX_H__*/
