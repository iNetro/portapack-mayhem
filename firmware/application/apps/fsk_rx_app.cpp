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

#include "fsk_rx_app.hpp"
#include "ui_modemsetup.hpp"

#include "modems.hpp"
#include "audio.hpp"
#include "io_file.hpp"
#include "rtc_time.hpp"
#include "baseband_api.hpp"
#include "string_format.hpp"
#include "portapack_persistent_memory.hpp"
#include "ui_fileman.hpp"
#include "ui_textentry.hpp"

using namespace portapack;
using namespace modems;
namespace fs = std::filesystem;

void FSKRxLogger::log_raw_data(const std::string& data) {
    log_file.write_entry(data);
}

std::string pad_string_with_spaces2(int snakes) {
    std::string paddedStr(snakes, ' ');
    return paddedStr;
}

namespace ui {

template <>
void RecentEntriesTable<FskRecentEntries>::draw(
    const Entry& entry,
    const Rect& target_rect,
    Painter& painter,
    const Style& style) {
    std::string line{};
    line.reserve(30);

    if (!entry.nameString.empty() && entry.include_name) {
        line = entry.nameString;

        if (line.length() < 17) {
            line += pad_string_with_spaces2(17 - line.length());
        } else {
            line = truncate(line, 17);
        }
    } else {
        line = to_string_mac_address(entry.packetData.deviceId, 6, false);
    }

    // Pushing single digit values down right justified.
    std::string hitsStr = to_string_dec_int(entry.numHits);
    int hitsDigits = hitsStr.length();
    uint8_t hits_spacing = 8 - hitsDigits;

    // Pushing single digit values down right justified.
    std::string dbStr = to_string_dec_int(entry.dbValue);
    int dbDigits = dbStr.length();
    uint8_t db_spacing = 5 - dbDigits;

    line += pad_string_with_spaces2(hits_spacing) + hitsStr;

    line += pad_string_with_spaces2(db_spacing) + dbStr;

    line.resize(target_rect.width() / 8, ' ');
    painter.draw_string(target_rect.location(), style, line);
}

static std::uint64_t get_freq_by_channel_number(uint8_t channel_number) {
    uint64_t freq_hz;

    switch (channel_number) {
        case 0 ... 15:
            freq_hz = 902'075'000ull + channel_number * 25'000ull;
            break;
        default:
            freq_hz = UINT64_MAX;
    }

    return freq_hz;
}

void FskRxView::focus() {
    options_channel.focus();
}

void FskRxView::file_error() {
    nav_.display_modal("Error", "File read error.");
}

FskRxView::FskRxView(NavigationView& nav)
    : nav_{nav} {
    baseband::run_image(portapack::spi_flash::image_tag_fskrx);

    add_children({&rssi,
                  &channel,
                  &field_rf_amp,
                  &field_lna,
                  &field_vga,
                  &options_channel,
                  &field_frequency,
                  &check_log,
                  &button_find,
                  &check_name,
                  &label_sort,
                  &options_sort,
                  &label_found,
                  &text_found_count,
                  &button_filter,
                  &button_clear_list,
                  &button_switch,
                  &recent_entries_view});

    filterBuffer = filter;

    button_filter.on_select = [this](Button&) {
        text_prompt(
            nav_,
            filterBuffer,
            64,
            [this](std::string& buffer) {
                on_filter_change(buffer);
            });
    };

    logger = std::make_unique<FSKRxLogger>();

    check_log.set_value(logging);

    check_log.on_select = [this](Checkbox&, bool v) {
        str_log = "";
        logging = v;

        if (logger && logging)
            logger->append(
                "FSKRX/Logs"
                "/FSKRXLOG_" +
                to_string_timestamp(rtc_time::now()) + ".TXT");
    };

    button_clear_list.on_select = [this](Button&) {
        recent.clear();
    };

    field_frequency.set_step(0);

    check_name.set_value(name_enable);

    check_name.on_select = [this](Checkbox&, bool v) {
        name_enable = v;
        setAllMembersToValue(recent, &FskRecentEntry::include_name, v);
        recent_entries_view.set_dirty();
    };

    options_channel.on_change = [this](size_t index, int32_t v) {
        channel_index = (uint8_t)index;

        // If we selected Auto don't do anything and Auto will handle changing.
        if (v == 40) {
            auto_channel = true;
            return;
        } else {
            auto_channel = false;
        }

        field_frequency.set_value(get_freq_by_channel_number(v));
        channel_number = v;

        baseband::set_fsk(channel_number);
    };

    options_sort.on_change = [this](size_t index, int32_t v) {
        sort_index = (uint8_t)index;
        handle_entries_sort(v);
    };

    options_channel.set_selected_index(channel_index, true);
    options_sort.set_selected_index(sort_index, true);

    button_find.on_select = [this](Button&) {
        auto open_view = nav_.push<FileLoadView>(".TXT");
        open_view->on_changed = [this](std::filesystem::path new_file_path) {
            on_file_changed(new_file_path);

            // nav_.set_on_pop([this]() { button_play.focus(); });
        };
    };

    // Auto-configure modem for LCR RX (will be removed later)
    baseband::set_fsk(channel_number);

    receiver_model.enable();
}

void FskRxView::on_data(FskPacketData* packet) {
    std::string str_console = "";

    if (!logging) {
        str_log = "";
    }

    str_console += "Device ID:";
    str_console += to_string_mac_address(packet->deviceId, 6, false);

    str_console += " Len:";
    str_console += to_string_dec_uint(packet->size);

    str_console += "\n";
    str_console += "Data:";

    int i;

    for (i = 0; i < packet->dataLen; i++) {
        str_console += to_string_hex(packet->data[i], 2);
    }

    str_console += "\n";

    // uint64_t macAddressEncoded = 0;//copy_mac_address_to_uint64(packet->deviceId);

    // // Start of Packet stuffing.
    // // Masking off the top 2 bytes to avoid invalid keys.
    // auto& entry = ::on_packet(recent, macAddressEncoded & 0xFFFFFFFFFFFF);
    // updateEntry(packet, entry);

    // // Add entries if they meet the criteria.
    // auto value = filter;
    // resetFilteredEntries(recent, [&value](const FskRecentEntry& entry) {
    //     return (entry.dataString.find(value) == std::string::npos) && (entry.nameString.find(value) == std::string::npos);
    // });

    // handle_entries_sort(options_sort.selected_index());

    // Log at End of Packet.
    if (logger && logging) {
        logger->log_raw_data(str_console);
    }
}

void FskRxView::on_filter_change(std::string value) {
    // New filter? Reset list from recent entries.
    if (filter != value) {
        resetFilteredEntries(recent, [&value](const FskRecentEntry& entry) {
            return (entry.dataString.find(value) == std::string::npos) && (entry.nameString.find(value) == std::string::npos);
        });
    }

    filter = value;
}

void FskRxView::on_file_changed(const std::filesystem::path& new_file_path) {
    file_path = fs::path(u"/") + new_file_path;
    found_count = 0;
    total_count = 0;
    searchList.clear();

    {  // Get the size of the data file.
        File data_file;
        auto error = data_file.open(file_path, true, false);
        if (error) {
            file_error();
            file_path = "";
            return;
        }

        uint64_t bytesRead = 0;
        uint64_t bytePos = 0;
        char currentLine[maxLineLength];

        do {
            memset(currentLine, 0, maxLineLength);

            bytesRead = readUntil(data_file, currentLine, maxLineLength, '\n');

            // Remove return if found.
            if (currentLine[strlen(currentLine)] == '\r') {
                currentLine[strlen(currentLine)] = '\0';
            }

            if (!bytesRead) {
                break;
            }

            searchList.push_back(currentLine);
            total_count++;

            bytePos += bytesRead;

        } while (bytePos <= data_file.size());
    }
}

// called each 1/60th of second, so 6 = 100ms
void FskRxView::on_timer() {
    if (++timer_count == timer_period) {
        timer_count = 0;

        if (auto_channel) {
            int min = 37;
            int max = 39;

            int randomChannel = min + std::rand() % (max - min + 1);

            field_frequency.set_value(get_freq_by_channel_number(randomChannel));
            baseband::set_fsk(randomChannel);
        }
    }
}

void FskRxView::handle_entries_sort(uint8_t index) {
    switch (index) {
        case 0:
            sortEntriesBy(
                recent, [](const FskRecentEntry& entry) { return entry.numHits; }, false);
            break;
        case 1:
            sortEntriesBy(
                recent, [](const FskRecentEntry& entry) { return entry.dbValue; }, false);
            break;
        case 2:
            sortEntriesBy(
                recent, [](const FskRecentEntry& entry) { return entry.timestamp; }, false);
            break;
        case 3:
            sortEntriesBy(
                recent, [](const FskRecentEntry& entry) { return entry.nameString; }, true);
            break;
        default:
            break;
    }

    recent_entries_view.set_dirty();
}

void FskRxView::set_parent_rect(const Rect new_parent_rect) {
    View::set_parent_rect(new_parent_rect);
    const Rect content_rect{0, header_height, new_parent_rect.width(), new_parent_rect.height() - header_height - switch_button_height};
    recent_entries_view.set_parent_rect(content_rect);
}

FskRxView::~FskRxView() {
    receiver_model.disable();
    baseband::shutdown();
}

void FskRxView::updateEntry(const FskPacketData* packet, FskRecentEntry& entry) {
    std::string data_string;

    int i;

    for (i = 0; i < packet->dataLen; i++) {
        data_string += to_string_hex(packet->data[i], 2);
    }

    entry.dbValue = packet->max_dB;
    entry.timestamp = to_string_timestamp(rtc_time::now());
    entry.dataString = data_string;

    entry.packetData.type = packet->type;
    entry.packetData.size = packet->size;
    entry.packetData.dataLen = packet->dataLen;

    // Mac Address of sender.
    entry.packetData.deviceId[0] = packet->deviceId[0];
    entry.packetData.deviceId[1] = packet->deviceId[1];
    entry.packetData.deviceId[2] = packet->deviceId[2];
    entry.packetData.deviceId[3] = packet->deviceId[3];
    entry.packetData.deviceId[4] = packet->deviceId[4];
    entry.packetData.deviceId[5] = packet->deviceId[5];

    entry.numHits++;
    entry.channelNumber = channel_number;

    // Parse Data Section into buffer to be interpretted later.
    for (int i = 0; i < packet->dataLen; i++) {
        entry.packetData.data[i] = packet->data[i];
    }

    entry.include_name = check_name.value();
}

} /* namespace ui */
