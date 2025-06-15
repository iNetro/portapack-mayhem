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

#ifndef __LRW_RX_APP_H__
#define __LRW_RX_APP_H__

#include "lrw_tx_app.hpp"

#include "ui.hpp"
#include "ui_navigation.hpp"
#include "ui_receiver.hpp"
#include "ui_freq_field.hpp"
#include "ui_record_view.hpp"
#include "app_settings.hpp"
#include "radio_state.hpp"
#include "log_file.hpp"
#include "utility.hpp"
#include "usb_serial_thread.hpp"
#include "file_path.hpp"

#include "recent_entries.hpp"
#include "message_tools/src/crc16.h"
#include "message_tools/src/lfsr.h"
#include "message_tools/src/tpc_encoder.h"
#include "message_tools/src/systematic_decode.h"

#define LRW_MESSAGE_SIZE 200

using namespace ui;

namespace ui {

struct LRWRecentEntry {
    using Key = uint32_t;

    static constexpr Key invalid_key = 0xffffffff;

    uint32_t deviceId;
    uint16_t msgType;
    int dbValue;
    uint8_t lrwData[LRW_MESSAGE_SIZE / 3];
    FskPacketData packetData;

    LRWRecentEntry()
        : LRWRecentEntry{0} {
    }

    LRWRecentEntry(
        const uint32_t deviceId)
        : deviceId{deviceId},
          msgType{},
          dbValue{},
          lrwData{},
          packetData{} {
    }

    Key key() const {
        return deviceId;
    }
};

using LRWRecentEntries = RecentEntries<LRWRecentEntry>;
using LRWRecentEntriesView = RecentEntriesView<LRWRecentEntries>;

class LRWRecentEntryDetailView : public View {
   public:
    LRWRecentEntryDetailView(NavigationView& nav, const LRWRecentEntry& entry);

    void set_entry(const LRWRecentEntry& new_entry);
    const LRWRecentEntry& entry() const { return entry_; };

    void update_data();
    void focus() override;
    void paint(Painter&) override;
    LRWTxPacket build_packet(LRWRecentEntry entry_);

   private:
    NavigationView& nav_;
    LRWRecentEntry entry_{};
    // void on_save_file(const std::string value, BLETxPacket packetToSave);
    // bool saveFile(const std::filesystem::path& path, BLETxPacket packetToSave);
    // std::string packetFileBuffer{};
    // std::filesystem::path packet_save_path{blerx_dir / u"Lists/????.csv"};

    static constexpr uint8_t total_data_lines{5};

    Labels label_device_id{
        {{0 * 8, 0 * 16}, "Device ID:", Theme::getInstance()->fg_light->foreground}};

    Text text_device_id{
        {10 * 8, 0 * 16, 17 * 8, 16},
        "-"};

    Labels label_msg_type{
        {{0 * 8, 1 * 16}, "Msg Type:", Theme::getInstance()->fg_light->foreground}};

    Text text_msg_type{
        {9 * 8, 1 * 16, 17 * 8, 16},
        "-"};

    Labels labels{
        {{0 * 8, 3 * 16}, "Message Data", Theme::getInstance()->fg_light->foreground},
    };

    Button button_done{
        {72, 264, 96, 24},
        "Done"};

    Button button_send{
        {19, 224, 96, 24},
        "Send"};

    Rect draw_field(
        Painter& painter,
        const Rect& draw_rect,
        const Style& style,
        const std::string& label,
        const std::string& value);
};

class LRWRxView : public View {
   public:
    LRWRxView(NavigationView& nav);
    ~LRWRxView();

    void set_parent_rect(const Rect new_parent_rect) override;
    void paint(Painter&) override{};

    void focus() override;

    std::string title() const override { return "LRW RX"; };
    static std::string pad_string_with_spaces(int snakes);
    static std::uint64_t get_freq_by_channel_number_fsk(uint8_t channel_number);

   private:
    void on_save_file(const std::string value);
    bool saveFile(const std::filesystem::path& path);
    std::unique_ptr<UsbSerialThread> usb_serial_thread{};
    void on_data_fsk(FskPacketData* packet);
    void on_filter_change(std::string value);
    void on_file_changed(const std::filesystem::path& new_file_path);
    void file_error();
    void on_timer();
    void handle_entries_sort(uint8_t index);
    void handle_filter_options(uint8_t index);
    void parse_lrw_data(const uint8_t* data, uint8_t length, std::string& nameString, std::string& versionString);
    void updateEntry(FskPacketData* packet, LRWRecentEntry& entry);
    void on_packet_waiting(void);

    NavigationView& nav_;

    RxRadioState radio_state_{
        902075000, /* frequency */
        960000,    /* bandwidth */
        960000,    /* sampling rate */
        ReceiverModel::Mode::Capture};

    uint8_t channel_index{0};
    uint8_t sort_index{0};
    uint8_t filter_index{0};
    std::string filter{};
    bool logging{false};
    bool serial_logging{false};
    bool async_tx_states_when_entered{false};

    bool name_enable{true};
    app_settings::SettingsManager settings_{
        "rx_lrw",
        app_settings::Mode::RX,
        {
            {"channel_index"sv, &channel_index},
            {"sort_index"sv, &sort_index},
            {"filter"sv, &filter},
            {"log"sv, &logging},
            {"filter_index"sv, &filter_index},
            // disabled to always start without USB serial activated until we can make it non blocking if not connected
            // {"serial_log"sv, &serial_logging},
            {"name"sv, &name_enable},
        }};

    std::string str_console = "";
    uint8_t console_color{0};
    uint32_t prev_value{0};
    uint8_t channel_number = 0;
    bool auto_channel = false;

    int16_t timer_count{0};
    int16_t timer_period{9};  // 150ms

    std::string filterBuffer{};
    std::string listFileBuffer{};
    std::string headerStr = "Timestamp, MAC Address, Name, Packet Type, Data, Hits, dB, Channel";
    uint16_t maxLineLength = 140;

    std::filesystem::path file_path{};
    uint64_t found_count = 0;
    uint64_t total_count = 0;
    std::vector<std::string> searchList{};

    std::filesystem::path find_packet_path{blerx_dir / u"Find/????.TXT"};
    std::filesystem::path log_packets_path{blerx_dir / u"Logs/????.TXT"};
    std::filesystem::path packet_save_path{blerx_dir / u"Lists/????.csv"};

    static constexpr auto header_height = 9 * 8;
    static constexpr auto switch_button_height = 3 * 16;

    OptionsField options_channel{
        {0 * 8, 0 * 8},
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

    RxFrequencyField field_frequency{
        {6 * 8, 0 * 16},
        nav_};

    RFAmpField field_rf_amp{
        {16 * 8, 0 * 16}};

    LNAGainField field_lna{
        {18 * 8, 0 * 16}};

    VGAGainField field_vga{
        {21 * 8, 0 * 16}};

    RSSI rssi{
        {24 * 8, 0, 6 * 8, 4}};

    Channel channel{
        {24 * 8, 5, 6 * 8, 4}};

    Labels label_sort{
        {{0 * 8, 2 * 8}, "Sort:", Theme::getInstance()->fg_light->foreground}};

    OptionsField options_sort{
        {5 * 8, 2 * 8},
        4,
        {{"ID", 0},
         {"Hits", 1},
         {"dB", 2},
         {"Time", 3},
         {"Name", 4}}};

    Button button_filter{
        {11 * 8, 2 * 8, 7 * 8, 16},
        "Filter:"};

    OptionsField options_filter{
        {18 * 8 + 2, 2 * 8},
        4,
        {{"Data", 0},
         {"ID", 1}}};

    Checkbox check_log{
        {10 * 8, 4 * 8 + 2},
        3,
        "Log",
        true};

    Checkbox check_name{
        {0 * 8, 4 * 8 + 2},
        3,
        "Name",
        true};

    Button button_find{
        {0 * 8, 7 * 8 - 2, 4 * 8, 16},
        "Find"};

    Labels label_found{
        {{5 * 8, 7 * 8 - 2}, "Found:", Theme::getInstance()->fg_light->foreground}};

    Text text_found_count{
        {11 * 8, 7 * 8 - 2, 20 * 8, 16},
        "0/0"};

    Checkbox check_serial_log{
        {18 * 8 + 2, 4 * 8 + 2},
        7,
        "USB Log",
        true};

    // Console console{
    //     {0, 10 * 8, 240, 240}};

    Button button_clear_list{
        {2 * 8, 320 - (16 + 32), 7 * 8, 32},
        "Clear"};

    Button button_save_list{
        {11 * 8, 320 - (16 + 32), 11 * 8, 32},
        "Export CSV"};

    Button button_switch{
        {240 - 6 * 8, 320 - (16 + 32), 4 * 8, 32},
        "Tx"};

    std::string str_log{""};

    LRWRecentEntries recent{};
    LRWRecentEntries tempList{};

#define DEVICE_ID_COLUMN_LENGTH 10
#define MSG_TYPE_COLUMN_LENGTH 5
#define OFFSET_COLUMN_LENGTH 7
#define VERSION_COLUMN_LENGTH 5

    const RecentEntriesColumns columns{{
        {"Device ID", 10},
        {"Msg.", 5},
        {"Offset", 7},
        {"Ver.", 5},
    }};

    LRWRecentEntriesView recent_entries_view{columns, recent};

    MessageHandlerRegistration message_handler_packet_fsk{
        Message::ID::FSKPacket,
        [this](Message* const p) {
            const auto message = static_cast<const FSKRxPacketMessage*>(p);
            this->on_data_fsk(message->packet);
        }};

    MessageHandlerRegistration message_handler_lrw_decoded_packet{
        Message::ID::LRWDecodedPacket,
        [this](Message* const p) {
            const auto message = static_cast<const LRWDecodeMessage*>(p);
            this->on_packet_waiting();
        }};

    MessageHandlerRegistration message_handler_frame_sync{
        Message::ID::DisplayFrameSync,
        [this](const Message* const) {
            this->on_timer();
        }};
}; /* LRWRxView */

} /* namespace ui */

#endif /*__UI_AFSK_RX_H__*/
