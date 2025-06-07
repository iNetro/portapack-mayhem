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
#include "lrw_tx_app.hpp"
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

static void encoder_init(void)
{
	static bool first_time = true;

	// WTB: What happens if the encoder changes at runtime? Can this occur?
	if (first_time)
	{
	    crc16_create_default();
        lfsr_create_default();
		tpc_encoder_create(TPC_72_40);
	}

	first_time = false;
}

static void decoder_init(void)
{
	encoder_init();
}

static void decode_radio_packet(uint8_t *input_data, uint16_t msg_len, uint8_t *output_buf, uint16_t *outLen)
{
	uint8_t decode_in_len = tpc_decoder_get_input_length_bytes(TPC_72_40);
	uint8_t decode_out_len = tpc_decoder_get_output_length_bytes(TPC_72_40);
	uint8_t num_blocks = msg_len / decode_in_len;
	num_blocks += msg_len % decode_in_len ? 1:0;
	*outLen = num_blocks * decode_out_len;

	decoder_init();

    for(int i = 0; i < num_blocks; i++)
    {
        systematic_decode(TPC_72_40, input_data + decode_in_len * i, output_buf + decode_out_len * i);
    }

    lfsr_reset();
    lfsr_whiten_bytes(output_buf, output_buf, msg_len);

	return;
}
namespace ui {

LRWRecentEntryDetailView::LRWRecentEntryDetailView(NavigationView& nav, const LRWRecentEntry& entry)
    : nav_{nav},
      entry_{entry} {
    add_children({&button_done,
                  &button_send,
                  &label_device_id,
                  &text_device_id,
                  &label_msg_type,
                  &text_msg_type,
                  &labels});

    text_device_id.set(to_string_dec_uint(entry.deviceId));
    text_msg_type.set(to_string_dec_uint(entry.msgType));

    button_done.on_select = [&nav](const ui::Button&) {
        nav.pop();
    };

    button_send.on_select = [this, &nav](const ui::Button&) {
        auto packetToSend = build_packet(entry_);
        nav.set_on_pop([packetToSend, &nav]() {
            nav.replace<LRWTxView>(packetToSend);
        });
        nav.pop();
    };
}

LRWTxPacket LRWRecentEntryDetailView::build_packet(LRWRecentEntry entry_) {
    LRWTxPacket lrwTxPacket;
    memset(&lrwTxPacket, 0, sizeof(LRWTxPacket));

    std::string deviceIdStr = to_string_dec_uint(entry_.deviceId);

    std::string data_string = "";

    int i;

    for (i = 0; i < 32; i++) {
        data_string += to_string_hex(entry_.lrwData[i], 2);
    }

    strncpy(lrwTxPacket.deviceId, deviceIdStr.c_str(), 10);
    strncpy(lrwTxPacket.advertisementData, data_string.c_str(), 32 * 2);
    strncpy(lrwTxPacket.packetCount, "10", 3);
    lrwTxPacket.packet_count = 10;

    return lrwTxPacket;
}

void LRWRecentEntryDetailView::update_data() {
}

void LRWRecentEntryDetailView::focus() {
    button_done.focus();
}

Rect LRWRecentEntryDetailView::draw_field(
    Painter& painter,
    const Rect& draw_rect,
    const Style& style,
    const std::string& label,
    const std::string& value) {
    const int label_length_max = 4;

    painter.draw_string(Point{draw_rect.left(), draw_rect.top()}, style, label);
    painter.draw_string(Point{draw_rect.left() + (label_length_max + 1) * 8, draw_rect.top()}, style, value);

    return {draw_rect.left(), draw_rect.top() + draw_rect.height(), draw_rect.width(), draw_rect.height()};
}

void LRWRecentEntryDetailView::paint(Painter& painter) {
    View::paint(painter);

    const auto s = style();
    const auto rect = screen_rect();

    auto field_rect = Rect{rect.left(), rect.top() + 64, rect.width(), 16};

    std::string dataString = "";
    std::string labelString = "";

    uint16_t totalVisableBytes = LRW_MESSAGE_SIZE / 3;

    for (int i = 0; i < totalVisableBytes; i += 12) {
        dataString = "";
        labelString = to_string_hex(i, 2);;
        for (int j = 0; j < 12 && (i + j) < totalVisableBytes; j++) {
            dataString += to_string_hex(entry_.lrwData[i + j], 2);
        }
        field_rect = draw_field(painter, field_rect, s, labelString, dataString);
    }
}

void LRWRecentEntryDetailView::set_entry(const LRWRecentEntry& entry) {
    entry_ = entry;
    set_dirty();
}

template <>
void RecentEntriesTable<LRWRecentEntries>::draw(
    const Entry& entry,
    const Rect& target_rect,
    Painter& painter,
    const Style& style) {
    std::string line{};
    line.reserve(30);

    std::string deviceIdStr = to_string_dec_uint(entry.deviceId);
    std::string msgTypeStr = to_string_dec_uint(entry.msgType);
    truncate(deviceIdStr, DEVICE_ID_COLUMN_LENGTH);
    truncate(msgTypeStr, MSG_TYPE_COLUMN_LENGTH);

    line = deviceIdStr + LRWRxView::pad_string_with_spaces(DEVICE_ID_COLUMN_LENGTH - deviceIdStr.length() + 1);
    line += msgTypeStr + LRWRxView::pad_string_with_spaces(MSG_TYPE_COLUMN_LENGTH - msgTypeStr.length() + 1);

    painter.draw_string(target_rect.location(), style, line);
}

std::string LRWRxView::pad_string_with_spaces(int snakes) {
    std::string paddedStr(snakes, ' ');
    return paddedStr;
}

std::uint64_t LRWRxView::get_freq_by_channel_number_fsk(uint8_t channel_number) {
    uint64_t freq_hz;

    freq_hz = 902'075'000ull + (channel_number) * 25'000ull;

    return freq_hz;
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

    recent_entries_view.on_select = [this](const LRWRecentEntry& entry) {
        nav_.push<LRWRecentEntryDetailView>(entry);
    };

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
        recent.clear();
    };

    button_switch.on_select = [&nav](Button&) {
        nav.replace<LRWTxView>();
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

    uint16_t decoded_msg_len = 0;
	uint8_t decoded_msg[LRW_MESSAGE_SIZE] = {0};

    decode_radio_packet(packet->data, packet->dataLen, decoded_msg, &decoded_msg_len);

    // str_console = "RAW Packet Data [Receiving]: \r\n";

    // for (int i = 0; i < packet->dataLen; i += 32) {
    //     str_console += "[ ";
    //     for (int j = 0; j < 32 && (i + j) < packet->dataLen; j++) {
    //         str_console += to_string_hex(packet->data[i + j]) + " ";
    //     }
    //     str_console += "]\r\n";
    // }

    for (int i = 0; i < decoded_msg_len; i += 32) {
        str_console += "[ ";
        for (int j = 0; j < 32 && (i + j) < decoded_msg_len; j++) {
            str_console += to_string_hex(decoded_msg[i + j]) + " ";
        }
        str_console += "]\r\n";
    }

    uint32_t device_ID = decoded_msg[2] << 24 | decoded_msg[3] << 16 | decoded_msg[4] << 8 | decoded_msg[5];

    auto& entry = ::on_packet(recent, device_ID & 0xFFFFFFFF);
    updateEntry(decoded_msg, entry);

    recent_entries_view.set_dirty();
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

void LRWRxView::updateEntry(uint8_t * decodedLrwData, LRWRecentEntry& entry) {

    entry.msgType = decodedLrwData[6] << 8 | decodedLrwData[7];

    for (int i = 0; i < LRW_MESSAGE_SIZE / 3; i++) {
        entry.lrwData[i] = decodedLrwData[i];
    }

    str_console += "Device ID: " + to_string_dec_uint(entry.deviceId) + "\r\n";

    if (serial_logging) {
        UsbSerialAsyncmsg::asyncmsg(str_console);  // new line handled there, no need here.
    }

    str_console = "";
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
