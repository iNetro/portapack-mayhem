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

#ifndef __FSK_RX_APP_H__
#define __FSK_RX_APP_H__

#include "ui.hpp"
#include "ui_navigation.hpp"
#include "ui_receiver.hpp"
#include "ui_freq_field.hpp"
#include "ui_record_view.hpp"
#include "app_settings.hpp"
#include "radio_state.hpp"
#include "log_file.hpp"
#include "utility.hpp"

#include "recent_entries.hpp"

class FSKRxLogger {
   public:
    Optional<File::Error> append(const std::string& filename) {
        return log_file.append(filename);
    }

    void log_raw_data(const std::string& data);

   private:
    LogFile log_file{};
};

namespace ui {

struct FskRecentEntry {
    using Key = uint64_t;

    static constexpr Key invalid_key = 0xffffffff;

    uint64_t deviceName;
    int dbValue;
    FskPacketData packetData;
    std::string timestamp;
    std::string dataString;
    std::string nameString;
    bool include_name;
    uint16_t numHits;
    uint8_t channelNumber;
    bool entryFound;

    FskRecentEntry()
        : FskRecentEntry{0} {
    }

    FskRecentEntry(
        const uint64_t deviceName)
        : deviceName{deviceName},
          dbValue{},
          packetData{},
          timestamp{},
          dataString{},
          nameString{},
          include_name{},
          numHits{},
          channelNumber{},
          entryFound{} {
    }

    Key key() const {
        return deviceName;
    }
};

using FskRecentEntries = RecentEntries<FskRecentEntry>;
using FskRecentEntriesView = RecentEntriesView<FskRecentEntries>;

class FskRxView : public View {
   public:
    FskRxView(NavigationView& nav);
    ~FskRxView();

    void set_parent_rect(const Rect new_parent_rect) override;
    void paint(Painter&) override{};

    void focus() override;

    std::string title() const override { return "FSK RX"; };

   private:
    void on_data(FskPacketData* packetData);
    void on_filter_change(std::string value);
    void on_file_changed(const std::filesystem::path& new_file_path);
    void file_error();
    void on_timer();
    void handle_entries_sort(uint8_t index);
    void updateEntry(const FskPacketData* packet, FskRecentEntry& entry);

    NavigationView& nav_;
    RxRadioState radio_state_{
        902'075'000 /* frequency */,
        100000 /* bandwidth */,
        100000 /* sampling rate */,
        ReceiverModel::Mode::WidebandFMAudio};

    uint8_t channel_index{0};
    uint8_t sort_index{0};
    std::string filter{};
    bool logging{false};
    bool name_enable{true};
    app_settings::SettingsManager settings_{
        "rx_fsk",
        app_settings::Mode::RX,
        {
            {"channel_index"sv, &channel_index},
            {"sort_index"sv, &sort_index},
            {"filter"sv, &filter},
            {"log"sv, &logging},
            {"name"sv, &name_enable},
        }};

    uint8_t console_color{0};
    uint32_t prev_value{0};
    uint8_t channel_number = 0;
    bool auto_channel = false;

    int16_t timer_count{0};
    int16_t timer_period{6};  // 100ms

    std::string filterBuffer{};
    std::string listFileBuffer{};

    uint16_t maxLineLength = 140;

    std::filesystem::path file_path{};
    uint64_t found_count = 0;
    uint64_t total_count = 0;
    std::vector<std::string> searchList{};

    static constexpr auto header_height = 4 * 16;
    static constexpr auto switch_button_height = 3 * 16;

    OptionsField options_channel{
        {0 * 8, 0 * 8},
        5,
        {{"Ch.0", 0},
        {"Ch.1", 1},
        {"Ch.2", 2},
        {"Ch.3", 3},
        {"Ch.4", 4},
        {"Ch.5", 5},
        {"Ch.6", 6},
        {"Ch.7", 7},
        {"Ch.8", 8},
        {"Ch.9", 9},
        {"Ch.10", 10},
        {"Ch.11", 11},
        {"Ch.12", 12},
        {"Ch.13", 13},
        {"Ch.14", 14},
        {"Ch.15", 15}}};

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
        {{0 * 8, 3 * 8}, "Sort:", Color::light_grey()}};

    OptionsField options_sort{
        {5 * 8, 3 * 8},
        4,
         {{"Hits", 0},
         {"dB", 1},
         {"Time", 3},
         {"Name", 4}}};

    Button button_filter{
        {11 * 8, 3 * 8, 4 * 8, 16},
        "Filter"};

    Checkbox check_log{
        {17 * 8, 3 * 8},
        3,
        "Log",
        true};

    Checkbox check_name{
        {23 * 8, 3 * 8},
        3,
        "Name",
        true};

    Button button_find{
        {0 * 8, 6 * 8, 4 * 8, 16},
        "Find"};

    Labels label_found{
        {{5 * 8, 6 * 8}, "Found:", Color::light_grey()}};

    Text text_found_count{
        {11 * 8, 3 * 16, 20 * 8, 16},
        "0/0"};

    Console console{
        {0, 4 * 16, 240, 240}};

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
    std::unique_ptr<FSKRxLogger> logger{};

    FskRecentEntries recent{};
    FskRecentEntries tempList{};

    const RecentEntriesColumns columns{{
        {"Device ID", 17},
        {"Hits", 7},
        {"dB", 4},
    }};

    FskRecentEntriesView recent_entries_view{columns, recent};

    MessageHandlerRegistration message_handler_packet{
        Message::ID::FskPacket,
        [this](Message* const p) {
            const auto message = static_cast<const FskPacketMessage*>(p);
            this->on_data(message->packet);
        }};

    MessageHandlerRegistration message_handler_frame_sync{
        Message::ID::DisplayFrameSync,
        [this](const Message* const) {
            this->on_timer();
        }};
};

} /* namespace ui */

#endif /*__UI_AFSK_RX_H__*/
