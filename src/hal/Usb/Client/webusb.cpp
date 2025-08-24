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

#include "webusb.hpp"
#include "tusb.h"
#include "usb_descriptors.h"
#include "hal/System/LockGuard.hpp"

#include <string>
#include <cstdint>
#include <cstddef>
#include <unordered_map>

// Packet format (big endian order):
// Magic Bytes [4] | Size [2] | Inverse Size [2] | Return Address [1-9] | Command [1] | Payload [0-N] | CRC [2]
// Size is number of bytes in return address, command, payload, and crc
// Return address must keep growing until most significant bit is 0, 9 bytes are read, or size has elapsed
// CRC covers return address, command, and payload

// Note: the purpose of Return Address is to tag each command so the host knows where to direct the response without
//       the need to serialize communication. The address may be up to 64 bits in size.

//! The magic value that every packet must begin with
static constexpr const std::uint8_t k_webusb_magic_value[] = {0xDB, 0x8B, 0xAF, 0xD5};

//! Mutex ensuring serialization to webusb output
MutexInterface* webusb_mutex = nullptr;

class WebUsbInterface
{
public:
    static constexpr const std::int8_t kSizeMagic = sizeof(k_webusb_magic_value);
    static constexpr const std::int8_t kSizeSize = 4;
    static constexpr const std::int8_t kMinSizeAddress = 1;
    static constexpr const std::int8_t kMaxSizeAddress = 9;
    static constexpr const std::int8_t kSizeCommand = 1;
    static constexpr const std::int8_t kSizeCrc = 2;

    static constexpr const std::uint8_t kCmdBadCmd = 0xFF;

public:
    WebUsbInterface() = delete;

    WebUsbInterface(uint8_t itf) : mItf(itf) {}

    void reset()
    {
        mRcvIdx = -kSizeMagic;
        mBuffer.clear();
        mBuffer.shrink_to_fit();
    }

    static void addParser(const std::shared_ptr<WebUsbCommandParser>& parser)
    {
        if (parser)
        {
            uint8_t cmd = parser->getSupportedCommand();
            mParsers[cmd] = parser;
        }
    }

    void process(const uint8_t* buffer, uint16_t bufsize)
    {
        while (bufsize > 0)
        {
            if (mRcvIdx < kSizeSize)
            {
                parseMagic(buffer, bufsize);

                while (mRcvIdx < kSizeSize && bufsize > 0)
                {
                    mSizeBytes[mRcvIdx] = *buffer;

                    ++mRcvIdx;
                    ++buffer;
                    --bufsize;
                }

                if (mRcvIdx < kSizeSize)
                {
                    // Consumed entire buffer without completing size bytes
                    return;
                }

                mRcvSize = bytesToUint16(&mSizeBytes[0]);
                std::uint16_t invRcvSize = bytesToUint16(&mSizeBytes[2]);

                if ((mRcvSize ^ invRcvSize) != 0xFFFF)
                {
                    // Size bytes invalid - reset counter, parse size bytes for another magic, and continue
                    reset();
                    const uint8_t* tmpBuffer = mSizeBytes;
                    uint16_t tmpBufSize = sizeof(mSizeBytes);
                    parseMagic(tmpBuffer, tmpBufSize);
                    continue;
                }

                mBuffer.reserve(mRcvSize);

                if (bufsize == 0)
                {
                    // Consumed size bytes then ran out of bytes to parse
                    return;
                }
            }

            std::uint16_t payloadIdx = mRcvIdx - kSizeSize;
            std::uint16_t bytesToConsume = mRcvSize - payloadIdx;
            if (bufsize < bytesToConsume)
            {
                bytesToConsume = bufsize;
            }

            mBuffer.append(buffer, buffer + bytesToConsume);

            mRcvIdx += bytesToConsume;
            bufsize -= bytesToConsume;

            payloadIdx = mRcvIdx - kSizeSize;

            if (payloadIdx >= mRcvSize)
            {
                if (mBuffer.size() < (kMinSizeAddress + kSizeCommand + kSizeCrc))
                {
                    // Not enough data for address, cmd, and CRC
                    return;
                }

                // Calculate CRC over message address, command, and payload (excluding CRC itself)
                uint16_t calc_crc = computeCrc16(
                    mBuffer.data(),
                    mBuffer.size() - kSizeCrc
                );

                // Extract CRC from last 2 bytes
                uint16_t pkt_crc = bytesToUint16(&mBuffer[mBuffer.size() - 2]);

                if (calc_crc == pkt_crc)
                {
                    uint64_t address = 0;
                    uint8_t addrIdx = 0;
                    do
                    {
                        uint8_t byte = mBuffer[addrIdx];
                        if (addrIdx != (kMaxSizeAddress - 1))
                        {
                            byte &= 0x7f;
                        }
                        address |= (byte << (addrIdx * 7));
                    } while (
                        (mBuffer[addrIdx++] & 0x80) != 0 &&
                        addrIdx < kMaxSizeAddress &&
                        addrIdx < mBuffer.size()
                    );

                    // addrIdx is now the number of bytes used by address
                    const uint8_t addrSize = addrIdx;
                    if (mBuffer.size() < static_cast<std::size_t>(addrSize + kSizeCommand + kSizeCrc))
                    {
                        // Not enough data for address, cmd, and CRC
                        return;
                    }

                    processPkt(
                        address,
                        mBuffer[addrSize],
                        reinterpret_cast<const uint8_t*>(&mBuffer[addrSize + kSizeCommand]),
                        mBuffer.size() - addrSize - kSizeCommand - kSizeCrc
                    );
                }

                // Done processing this packet
                reset();
            }
        }
    }

private:
    void processPkt(const uint64_t address, const uint8_t cmd, const uint8_t* payload, uint16_t payloadLen)
    {
        std::unordered_map<std::uint8_t, std::shared_ptr<WebUsbCommandParser>>::iterator iter = mParsers.find(cmd);
        if (iter != mParsers.end() && iter->second)
        {
            const uint8_t itf = mItf;
            iter->second->process(
                payload,
                payloadLen,
                [itf, address](std::uint8_t responseCmd, const std::list<std::pair<const void*, std::uint16_t>>& payloadList) -> void
                {
                    sendPkt(itf, address, responseCmd, payloadList);
                }
            );
        }
        else
        {
            // Unsupported command
            sendPkt(mItf, address, kCmdBadCmd, {{&cmd, sizeof(cmd)}});
        }
    }

    static void vendorWrite(std::uint8_t itf, const void* buffer, std::uint32_t bufsize, bool flush = false)
    {
        const std::uint8_t* buffer8 = reinterpret_cast<const std::uint8_t*>(buffer);
        while (bufsize > 0)
        {
            std::uint32_t written = tud_vendor_n_write(itf, buffer8, bufsize);
            written = (written >= bufsize) ? bufsize : written;
            bufsize -= written;
            buffer8 += written;

            if (bufsize > 0 || flush)
            {
                tud_vendor_n_write_flush(itf);
            }
        }
    }

    static void sendPkt(
        const uint8_t itfIdx,
        uint64_t address,
        const uint8_t cmd,
        const std::list<std::pair<const void*, std::uint16_t>>& payloadList
    )
    {
        std::uint16_t payloadLen = 0;
        for (const auto& it : payloadList)
        {
            payloadLen += it.second;
        }

        // Unpack address
        std::string addrBytes;
        addrBytes.reserve(kMaxSizeAddress);
        uint8_t addrIdx = 0;
        do
        {
            uint8_t byte = address;
            if (addrIdx != (kMaxSizeAddress - 1))
            {
                byte &= 0x7f;
                address >>= 8;
            }
            addrBytes.push_back(byte);
        } while (++addrIdx < kMaxSizeAddress && address != 0);

        const std::uint16_t pktSize = addrBytes.size() + kSizeCommand + payloadLen + kSizeCrc;
        const std::uint16_t invPktSize = pktSize ^ 0xFFFF;
        std::uint8_t headerSize = static_cast<std::uint8_t>(kSizeMagic + kSizeSize + addrBytes.size() + kSizeCommand);
        std::uint8_t header[headerSize];
        memcpy(&header[0], k_webusb_magic_value, kSizeMagic);
        uint16ToBytes(&header[kSizeMagic], pktSize);
        uint16ToBytes(&header[kSizeMagic + sizeof(pktSize)], invPktSize);
        memcpy(&header[kSizeMagic + kSizeSize], addrBytes.data(), addrBytes.size());
        header[kSizeMagic + kSizeSize + addrBytes.size()] = cmd;

        // Calculate CRC over message address, command, and payload (excluding CRC itself)
        uint16_t crc = computeCrc16(&header[kSizeMagic + kSizeSize], headerSize - (kSizeMagic + kSizeSize));
        for (const auto& it : payloadList)
        {
            crc = computeCrc16(crc, it.first, it.second);
        }
        std::uint8_t crcBuffer[kSizeCrc];
        uint16ToBytes(crcBuffer, crc);

        {
            LockGuard lock(*webusb_mutex);

            vendorWrite(itfIdx, header, headerSize);
            for (const auto& it : payloadList)
            {
                vendorWrite(itfIdx, it.first, it.second);
            }
            vendorWrite(itfIdx, crcBuffer, sizeof(crcBuffer), true);
        }
    }

    static std::uint16_t bytesToUint16(const void* payload)
    {
        const std::uint8_t* p8 = reinterpret_cast<const std::uint8_t*>(payload);
        return (static_cast<std::uint16_t>(p8[0]) << 8 | p8[1]);
    }

    static void uint16ToBytes(void* out, std::uint16_t data)
    {
        std::uint8_t* p8 = reinterpret_cast<std::uint8_t*>(out);
        *p8++ = data >> 8;
        *p8 = data & 0xFF;
    }

    static std::uint16_t computeCrc16(std::uint16_t seed, const void* buffer, std::uint16_t bufLen)
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

    static std::uint16_t computeCrc16(const void* buffer, std::uint16_t bufLen)
    {
        return computeCrc16(0xFFFFU, buffer, bufLen);
    }

    void parseMagic(const uint8_t*& buffer, uint16_t& bufsize)
    {
        while (mRcvIdx < 0 && bufsize > 0)
        {
            if (*buffer != k_webusb_magic_value[kSizeMagic + mRcvIdx])
            {
                // reset and keep waiting
                reset();
            }
            else
            {
                ++mRcvIdx;
            }

            ++buffer;
            --bufsize;
        }
    }

private:
    //! All parsers
    static std::unordered_map<std::uint8_t, std::shared_ptr<WebUsbCommandParser>> mParsers;

    //! The USB vendor interface number
    const uint8_t mItf;

    //! Current receive index
    std::int32_t mRcvIdx = -kSizeMagic;

    //! Received packet size
    std::uint8_t mSizeBytes[4] = {};

    //! Received size
    std::uint16_t mRcvSize = 0;

    //! Current receive buffer
    std::string mBuffer = std::string();
};

// definition of mParsers
std::unordered_map<std::uint8_t, std::shared_ptr<WebUsbCommandParser>> WebUsbInterface::mParsers;

//! All available interfaces, mapped by interface number
static std::unordered_map<std::uint8_t, WebUsbInterface> webusb_interfaces;

void webusb_init(MutexInterface* mutex)
{
    webusb_mutex = mutex;
}

void webusb_connection_event(uint16_t interfaceNumber, uint16_t value)
{
    uint8_t index = ITF_TO_WEBUSB_IDX(interfaceNumber);
    if (index < CFG_TUD_VENDOR)
    {
        bool connected = (value != 0);
        if (connected)
        {
            std::unordered_map<std::uint8_t, WebUsbInterface>::iterator iter = webusb_interfaces.find(index);
            if (iter != webusb_interfaces.end())
            {
                iter->second.reset();
            }
        }
        else
        {
            webusb_interfaces.erase(index);
        }
    }
}

void webusb_process(uint8_t itfIndex, const uint8_t* buffer, uint16_t bufsize)
{
    std::unordered_map<std::uint8_t, WebUsbInterface>::iterator iter = webusb_interfaces.find(itfIndex);
    if (iter == webusb_interfaces.end())
    {
        iter = webusb_interfaces.insert(std::make_pair(itfIndex, WebUsbInterface(itfIndex))).first;
    }

    iter->second.process(buffer, bufsize);
}

void webusb_add_parser(std::shared_ptr<WebUsbCommandParser> parser)
{
    WebUsbInterface::addParser(parser);
}
