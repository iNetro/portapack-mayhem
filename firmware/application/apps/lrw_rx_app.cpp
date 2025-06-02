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

#include "lrw_rx_app.hpp"
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
#include "usb_serial_asyncmsg.hpp"

using namespace portapack;
using namespace modems;
namespace fs = std::filesystem;

#define BLE_RX_NO_ERROR 0
#define BLE_RX_LIST_FILENAME_EMPTY_ERROR 1
#define BLE_RX_ENTRY_FILENAME_EMPTY_ERROR 2
#define BLE_RX_LIST_SAVE_ERROR 3
#define BLE_RX_ENTRY_SAVE_ERROR 4

namespace ui {

static std::uint64_t get_freq_by_channel_number_fsk(uint8_t channel_number) {
    uint64_t freq_hz;

    freq_hz = 902'073'750ull + (channel_number) * 25'000ull;

    return freq_hz;
}

template <>
void RecentEntriesTable<LRWRecentEntries>::draw(
    const Entry& entry,
    const Rect& target_rect,
    Painter& painter,
    const Style& style) {
    std::string line{};
    line.reserve(30);

    painter.draw_string(target_rect.location(), style, line);
}

void LRWRxView::focus() {
    options_channel.focus();
}

void LRWRxView::file_error() {
    nav_.display_modal("Error", "File read error.");
}

LRWRxView::LRWRxView(NavigationView& nav)
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
                  &check_serial_log,
                  &button_filter,
                  &options_filter,
                  &button_save_list,
                  &button_clear_list,
                  &button_switch,
                  &recent_entries_view});

    async_tx_states_when_entered = portapack::async_tx_enabled;

    baseband::set_fsk(7500, 10);
 
    check_serial_log.on_select = [this](Checkbox&, bool v) {
        serial_logging = v;
        if (v) {
            portapack::async_tx_enabled = true;
        } else {
            portapack::async_tx_enabled = false;
        }
    };
    check_serial_log.set_value(serial_logging);

    ensure_directory(find_packet_path);
    ensure_directory(log_packets_path);
    ensure_directory(packet_save_path);

    filterBuffer = filter;

    button_filter.on_select = [this](Button&) {
        text_prompt(
            nav_,
            filterBuffer,
            64,
            ENTER_KEYBOARD_MODE_ALPHA,
            [this](std::string& buffer) {
                on_filter_change(buffer);
            });
    };

    check_log.on_select = [this](Checkbox&, bool v) {
        logging = v;
    };

    check_log.set_value(logging);

    button_save_list.on_select = [this, &nav](const ui::Button&) {
        listFileBuffer = "";
        text_prompt(
            nav,
            listFileBuffer,
            64,
            ENTER_KEYBOARD_MODE_ALPHA,
            [this](std::string& buffer) {
                on_save_file(buffer);
            });
    };

    button_clear_list.on_select = [this](Button&) {
    };

    button_switch.on_select = [&nav](Button&) {
        nav.replace<BLETxView>();
    };

    field_frequency.set_step(0);

    check_name.set_value(name_enable);

    check_name.on_select = [this](Checkbox&, bool v) {
        name_enable = v;
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

        field_frequency.set_value(get_freq_by_channel_number_fsk(v));
        channel_number = v;

        baseband::set_btlerx(channel_number);
    };

    options_sort.on_change = [this](size_t index, int32_t v) {
        sort_index = (uint8_t)index;
        handle_entries_sort(v);
    };

    options_filter.on_change = [this](size_t index, int32_t v) {
        filter_index = (uint8_t)index;
        handle_filter_options(v);
    };

    options_channel.set_selected_index(channel_index, true);
    options_sort.set_selected_index(sort_index, true);
    options_filter.set_selected_index(filter_index, true);

    button_find.on_select = [this](Button&) {
        auto open_view = nav_.push<FileLoadView>(".TXT");
        open_view->on_changed = [this](std::filesystem::path new_file_path) {
            on_file_changed(new_file_path);

            // nav_.set_on_pop([this]() { button_play.focus(); });
        };
    };

    receiver_model.enable();
}

void LRWRxView::on_save_file(const std::string value) {
}

bool LRWRxView::saveFile(const std::filesystem::path& path) {
    return BLE_RX_NO_ERROR;
}

void LRWRxView::on_data_fsk(FskPacketData* packet) {

    str_console = "RAW Packet Data [Receiving]: \r\n";

    for (int i = 0; i < packet->dataLen; i += 32) {
        str_console += "[ ";
        for (int j = 0; j < 32 && (i + j) < packet->dataLen; j++) {
            str_console += to_string_hex(packet->data[i + j]) + " ";
        }
        str_console += "]\r\n";
    }

    if (serial_logging) {
        UsbSerialAsyncmsg::asyncmsg(str_console);  // new line handled there, no need here.
    }

    str_console = "";
}

void LRWRxView::on_filter_change(std::string value) {
    // New filter? Reset list from recent entries.
    if (filter != value) {
        filter = value;
        handle_filter_options(options_filter.selected_index());
    }
}

void LRWRxView::on_file_changed(const std::filesystem::path& new_file_path) {
    file_path = new_file_path;
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
void LRWRxView::on_timer() {
    if (++timer_count == timer_period) {
        timer_count = 0;

        if (auto_channel) {
            int min = 0;
            int max = 15;

            int randomChannel = min + std::rand() % (max - min + 1);

            field_frequency.set_value(get_freq_by_channel_number_fsk(randomChannel));
        }
    }
}

void LRWRxView::handle_entries_sort(uint8_t index) {
    switch (index) {
        case 0:
            break;
        case 1:
            break;
        case 2:
            break;
        case 3:
            break;
        case 4:
            break;
        default:
            break;
    }
}

void LRWRxView::handle_filter_options(uint8_t index) {
    auto value = filter;
    switch (index) {
        case 0:  // filter by Data
            break;
        case 1:  // filter by MAC address (All caps: e.g. AA:BB:CC:DD:EE:FF)
            break;
        case 2:  // filter by MAC address (All caps: e.g. AA:BB:CC:DD:EE:FF)
            break;
        default:
            break;
    }
}

void LRWRxView::set_parent_rect(const Rect new_parent_rect) {
    View::set_parent_rect(new_parent_rect);
    const Rect content_rect{0, header_height, new_parent_rect.width(), new_parent_rect.height() - header_height - switch_button_height};
    recent_entries_view.set_parent_rect(content_rect);
}

LRWRxView::~LRWRxView() {
    portapack::async_tx_enabled = async_tx_states_when_entered;
    receiver_model.disable();
    baseband::shutdown();
}

void LRWRxView::parse_lrw_data(const uint8_t* data, uint8_t length, std::string& nameString, std::string& versionString) {
}

} /* namespace ui */
