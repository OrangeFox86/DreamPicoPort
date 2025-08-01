// MIT License
//
// Copyright (c) 2022-2025 James Smith of OrangeFox86
// https://github.com/OrangeFox86/DreamcastControllerUsbPico
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "tusb.h"
#include "usb_descriptors.h"

#include <string>
#include <cstdint>
#include <cstddef>

// Packet format (big endian order):
// Magic Bytes [4] | Size [2] | Inverse Size [2] | Message Index [2] | Command [2] | Payload [0-N] | CRC [2]
// CRC covers message index, command, and payload

static constexpr const std::int8_t k_webusb_magic_size = 4;
static constexpr const std::int8_t k_webusb_size_size = 4;
static constexpr const std::int8_t k_webusb_idx_size = 2;
static constexpr const std::int8_t k_webusb_cmd_size = 2;
static constexpr const std::int8_t k_webusb_crc_size = 2;

static constexpr const std::uint16_t k_invalid_idx_value = 0xFFFF;
static constexpr const std::uint16_t k_cmd_bad_crc = 0xFFFF;


//! The magic value that every packet must begin with
static constexpr const std::uint8_t webusb_magic[k_webusb_magic_size] = {0xDB, 0x8B, 0xAF, 0xD5};

//! WebUSB connection state
static bool winusb_connected = false;

//! Current receive index
static std::int32_t webusb_rcv_idx = -k_webusb_magic_size;

//! Received packet size
static std::uint8_t webusb_size_bytes[4];

//! Received size
static std::uint16_t webusb_rcv_size;

//! Current receive buffer
static std::string webusb_buffer;

void webusb_reset_pkt()
{
    webusb_rcv_idx = -k_webusb_magic_size;
    webusb_buffer.clear();
    webusb_buffer.shrink_to_fit();
}

void webusb_connection_event(bool connected)
{
    if (winusb_connected != connected)
    {
        winusb_connected = connected;
        webusb_reset_pkt();
    }
}

static std::uint16_t bytes_to_uint16(const void* payload)
{
    const std::uint8_t* p8 = reinterpret_cast<const std::uint8_t*>(payload);
    return (static_cast<std::uint16_t>(p8[0]) << 8 | p8[1]);
}

static void uint16_to_bytes(void* out, std::uint16_t data)
{
    std::uint8_t* p8 = reinterpret_cast<std::uint8_t*>(out);
    *p8 = data >> 8;
    *p8 = data & 0xFF;
}

static std::uint16_t compute_crc16(std::uint16_t seed, const void* buffer, std::uint16_t bufLen)
{
    std::uint16_t crc = seed;
    const std::uint8_t* b8 = reinterpret_cast<const std::uint8_t*>(buffer);

    for (std::uint16_t i = 0; i < bufLen; ++i)
    {
        crc ^= static_cast<uint8_t>(*b8++) << 8;
        for (int j = 0; j < 8; ++j)
        {
            if (crc & 0x8000)
            {
                crc = (crc << 1) ^ 0x1021;
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    return crc;
}

static std::uint16_t compute_crc16(const void* buffer, std::uint16_t bufLen)
{
    return compute_crc16(0xFFFF, buffer, bufLen);
}

static void webusb_send_pkt(const uint16_t idx, const uint16_t cmd, const uint8_t* payload, uint16_t payloadLen)
{
    const std::uint16_t pktSize = k_webusb_idx_size + k_webusb_cmd_size + payloadLen + k_webusb_crc_size;
    const std::uint16_t invPktSize = pktSize ^ 0xFFFF;
    std::uint8_t header[k_webusb_magic_size + k_webusb_size_size + k_webusb_idx_size + k_webusb_cmd_size];
    memcpy(&header[0], webusb_magic, k_webusb_magic_size);
    uint16_to_bytes(&header[k_webusb_magic_size], pktSize);
    uint16_to_bytes(&header[k_webusb_magic_size + sizeof(pktSize)], invPktSize);
    uint16_to_bytes(&header[k_webusb_magic_size + k_webusb_size_size], idx);
    uint16_to_bytes(&header[k_webusb_magic_size + k_webusb_size_size + k_webusb_idx_size], cmd);

    // Calculate CRC over message index, command, and payload (excluding CRC itself)
    uint16_t crc = compute_crc16(header, sizeof(header));
    crc = compute_crc16(crc, payload, payloadLen);
    std::uint8_t crcBuffer[k_webusb_crc_size];
    uint16_to_bytes(crcBuffer, crc);

    tud_vendor_n_write(0, header, sizeof(header));
    tud_vendor_n_write(0, payload, payloadLen);
    tud_vendor_n_write(0, crcBuffer, sizeof(crcBuffer));
    tud_vendor_n_write_flush(0);
}

static void webusb_process_pkt(const uint16_t idx, const uint16_t cmd, const uint8_t* payload, uint16_t payloadLen)
{
    // Echo back
    webusb_send_pkt(idx, cmd, payload, payloadLen);
}

static void webusb_parse_magic(const uint8_t*& buffer, uint16_t& bufsize)
{
    while (webusb_rcv_idx < 0 && bufsize > 0)
    {
        if (*buffer != webusb_magic[k_webusb_magic_size + webusb_rcv_idx])
        {
            // reset and keep waiting
            webusb_reset_pkt();
        }
        else
        {
            ++webusb_rcv_idx;
        }

        ++buffer;
        --bufsize;
    }
}

void webusb_process(uint8_t itf, const uint8_t* buffer, uint16_t bufsize)
{
    while (bufsize > 0)
    {
        if (webusb_rcv_idx < k_webusb_size_size)
        {
            webusb_parse_magic(buffer, bufsize);

            while (webusb_rcv_idx < k_webusb_size_size && bufsize > 0)
            {
                webusb_size_bytes[webusb_rcv_idx] = *buffer;

                ++webusb_rcv_idx;
                ++buffer;
                --bufsize;
            }

            if (webusb_rcv_idx < k_webusb_size_size)
            {
                // Consumed entire buffer without completing size bytes
                return;
            }

            webusb_rcv_size = bytes_to_uint16(&webusb_size_bytes[0]);
            std::uint16_t invRcvSize = bytes_to_uint16(&webusb_size_bytes[2]);

            if ((webusb_rcv_size ^ invRcvSize) != 0xFFFF)
            {
                // Size bytes invalid - reset counter, parse size bytes for another magic, and continue
                webusb_reset_pkt();
                const uint8_t* tmpBuffer = webusb_size_bytes;
                uint16_t tmpBufSize = sizeof(webusb_size_bytes);
                webusb_parse_magic(tmpBuffer, tmpBufSize);
                continue;
            }

            webusb_buffer.reserve(webusb_rcv_size);

            if (bufsize == 0)
            {
                // Consumed size bytes then ran out of bytes to parse
                return;
            }
        }

        std::uint16_t bytesToConsume = webusb_rcv_size - webusb_rcv_idx;
        if (bufsize < bytesToConsume)
        {
            bytesToConsume = bufsize;
        }

        webusb_buffer.append(buffer, buffer + bytesToConsume);

        webusb_rcv_idx += bytesToConsume;
        bufsize -= bytesToConsume;

        if (webusb_rcv_idx >= webusb_rcv_size)
        {
            if (webusb_buffer.size() < (k_webusb_idx_size + k_webusb_cmd_size + k_webusb_crc_size))
            {
                // Not enough data for idx, cmd, and CRC
                return;
            }

            // Calculate CRC over message index, command, and payload (excluding CRC itself)
            uint16_t calc_crc = compute_crc16(
                webusb_buffer.data(),
                webusb_buffer.size() - k_webusb_crc_size
            );

            // Extract CRC from last 2 bytes
            uint16_t pkt_crc = bytes_to_uint16(&webusb_buffer[webusb_buffer.size() - 2]);

            if (calc_crc == pkt_crc)
            {
                webusb_process_pkt(
                    bytes_to_uint16(&webusb_buffer[0]),
                    bytes_to_uint16(&webusb_buffer[k_webusb_idx_size]),
                    reinterpret_cast<const uint8_t*>(&webusb_buffer[k_webusb_idx_size + k_webusb_cmd_size]),
                    webusb_buffer.size() - k_webusb_idx_size - k_webusb_cmd_size - k_webusb_crc_size
                );
            }

            // Done processing this packet
            webusb_reset_pkt();
        }
    }
}
