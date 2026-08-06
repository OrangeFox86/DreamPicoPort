// MIT License
//
// Copyright (c) 2022-2026 The DreamPicoPort Contributors
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
#include "tusb_config.h"
#include "usb_descriptors.h"
#include "hal/System/LockGuard.hpp"
#include "utils.h"

#include <string>
#include <cstdint>
#include <cstddef>
#include <unordered_map>
#include <array>
#include <vector>
#include <atomic>
#include <algorithm>
#include <memory>
#include <deque>

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
    static constexpr std::int8_t kSizeMagic = sizeof(k_webusb_magic_value);
    static constexpr std::int8_t kSizeSize = 4;
    static constexpr std::int8_t kMinSizeAddress = 1;
    static constexpr std::int8_t kMaxSizeAddress = 9;
    static constexpr std::int8_t kSizeCommand = 1;
    static constexpr std::int8_t kSizeCrc = 2;

    static constexpr std::uint8_t kCmdBadCmd = 0xFF;

    //! Limit number of outgoing packets to 10 just in case there is a lockup on the processing core
    static constexpr std::size_t kMaxOutgoingSize = 10;

    //! Maximum size of mIncomingBuffer before incoming data is thrown out
    static constexpr std::size_t kMaxBufferSize = 1100;

public:
    //! Default constructor (deleted)
    WebUsbInterface() = delete;

    //! Constructor
    //! @param[in] itf The interface index
    WebUsbInterface(uint8_t itf) : mItf(itf) {}

    //! Fully reset due to external event
    //! @param[in] sendZeros When true, send at least 256 zeros or number of zeros equivalent to last send size.
    //!                      This ensures that anything stuck in the USB output buffer is fully purged.
    //! @param[in] sendNullPkt When true, sends a "null packet" with command of 0 and maximum address value.
    //!                        The host may use this to synchronize to the beginning of the stream.
    void externalReset(bool sendZeros = false, bool sendNullPkt = false)
    {
        LockGuard lock(*webusb_mutex);
        reset();
        mIncomingBuffer.clear();
        mIncomingBuffer.shrink_to_fit();
        mOutgoingQueue.clear();
        mOutgoingQueue.shrink_to_fit();

        if (sendZeros && mLastSendSize >= 0)
        {
            // Write a bunch of zeros to ensure the last command gets pushed through if it got stuck
            std::uint32_t writeSize = std::max(static_cast<uint32_t>(mLastSendSize), static_cast<uint32_t>(256U));
            mLastSendSize = -1;
            uint8_t buff[512] = {};
            while (writeSize > 0)
            {
                std::uint32_t s = std::min(static_cast<std::uint32_t>(sizeof(buff)), writeSize);
                vendorWrite(mItf, buff, s, false);
                writeSize -= s;
            }
            // to ensure crc gets processed too
            vendorWrite(mItf, buff, 16, true);
        }

        if (sendNullPkt)
        {
            // Send command 0 with max address
            // This helps synchronize the beginning of the stream
            std::string maxAddr(9, static_cast<char>(0xFF));
            sendPkt(maxAddr, 0, {}, false);
        }
    }

    //! Perform connection operations
    //! @param[in] sendZeros See externalReset()
    //! @param[in] sendNullPkt See externalReset()
    void connect(bool sendZeros, bool sendNullPkt)
    {
        externalReset(sendZeros, sendNullPkt);
    }

    //! Perform disconnection operations
    void disconnect()
    {
        externalReset();
        // Proper disconnection means last send size no longer needs to be tracked
        mLastSendSize = -1;
    }

    //! Reset internal buffers
    void reset()
    {
        mRcvIdx = -kSizeMagic;
        mBuffer.clear();
    }

    //! Add a parser to my static parser dictionary
    //! @param[in] parser The parser to add
    static void addParser(const std::shared_ptr<WebUsbCommandHandler>& parser)
    {
        if (parser)
        {
            uint8_t cmd = parser->getSupportedCommand();
            mParsers[cmd] = parser;
        }
    }

    //! Add data from the incoming stream
    //! @param[in] buffer Pointer to the stream
    //! @param[in] bufsize Number of bytes to read
    void addData(const uint8_t* buffer, uint16_t bufsize)
    {
        if (bufsize > 0)
        {
            LockGuard lock(*webusb_mutex);

            // Limit bufsize if overflow will occur
            if (mIncomingBuffer.size() + bufsize > kMaxBufferSize)
            {
                // Just throw data away - rely on magic and CRC to recover
                bufsize = kMaxBufferSize - mIncomingBuffer.size();
            }

            mIncomingBuffer.insert(mIncomingBuffer.end(), buffer, buffer + bufsize);
        }
    }

    //! Called periodically to process all incoming data
    void process()
    {
        std::vector<std::uint8_t> newData;
        LockGuard lock(*webusb_mutex);

        if (!mIncomingBuffer.empty())
        {
            newData = std::move(mIncomingBuffer);
            mIncomingBuffer.clear();
        }

        const uint8_t* buffer = newData.data();
        std::size_t bufsize = newData.size();

        while (bufsize > 0)
        {
            if (!tryReadPacketHeader(buffer, bufsize))
            {
                // Consumed entire buffer without completing packet header.
                return;
            }

            consumePacketBytes(buffer, bufsize);

            const std::uint16_t payloadIdx = mRcvIdx - kSizeSize;
            if (payloadIdx >= mRcvSize)
            {
                if (!tryProcessCompletedPacket(lock))
                {
                    return;
                }
            }
        }
    }

    //! Called periodically to ensure all waiting data is sent to the USB outgoing buffer
    void flushOutgoing()
    {
        while (true)
        {
            std::vector<std::uint8_t> pkt;

            {
                LockGuard lock(*webusb_mutex);
                if (mOutgoingQueue.empty())
                {
                    return;
                }
                pkt = mOutgoingQueue.front();
                mOutgoingQueue.pop_front();
            }

            vendorWrite(mItf, pkt.data(), static_cast<uint32_t>(pkt.size()), true, true);
        }
    }

private:
    //! Consume bytes until all data is consumed or until full header is read (magic + size bytes)
    //! @param[in,out] buffer in: pointer to the beginning of the buffer to consume; out: incremented pointer
    //! @param[in,out] bufsize in: number of bytes to consume; out: number of bytes left to consume
    //! @return true if a full header is received
    //! @return false if bufsize is 0 and a full header has not been received yet
    bool tryReadPacketHeader(const uint8_t*& buffer, std::size_t& bufsize)
    {
        while (mRcvIdx < kSizeSize)
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
                return false;
            }

            mRcvSize = bytesToUint16(&mSizeBytes[0]);
            const std::uint16_t invRcvSize = bytesToUint16(&mSizeBytes[2]);

            if ((mRcvSize ^ invRcvSize) != 0xFFFF || mRcvSize < (kMinSizeAddress + kSizeCommand + kSizeCrc))
            {
                // Size bytes invalid - reset counter, parse size bytes for another magic, and continue.
                reset();
                const uint8_t* tmpBuffer = mSizeBytes;
                std::size_t tmpBufSize = sizeof(mSizeBytes);
                parseMagic(tmpBuffer, tmpBufSize);
                continue;
            }

            mBuffer.clear();
            mBuffer.reserve(mRcvSize);
            return true;
        }

        return true;
    }

    //! Called after header bytes have been consumed in order to pull bytes into address/payload buffer
    //! @param[in,out] buffer in: pointer to the beginning of the buffer to consume; out: incremented pointer
    //! @param[in,out] bufsize in: number of bytes to consume; out: number of bytes left to consume
    void consumePacketBytes(const uint8_t*& buffer, std::size_t& bufsize)
    {
        // mRcvIdx is guaranteed to be >= kSizeSize here.
        const std::uint16_t payloadIdx = mRcvIdx - kSizeSize;
        std::uint16_t bytesToConsume = mRcvSize - payloadIdx;
        if (bufsize < bytesToConsume)
        {
            bytesToConsume = bufsize;
        }

        mBuffer.insert(mBuffer.end(), buffer, buffer + bytesToConsume);

        mRcvIdx += bytesToConsume;
        buffer += bytesToConsume;
        bufsize -= bytesToConsume;
    }

    //! Determines how many bytes in a packet are the address
    //! @param[in] packet Packet data to check
    //! @return number of bytes in the packet that are the address
    static std::uint8_t getAddressSize(const std::vector<std::uint8_t>& packet)
    {
        std::uint8_t addrSize = 0;
        while (addrSize < kMaxSizeAddress && addrSize < packet.size())
        {
            const std::uint8_t b = packet[addrSize];
            ++addrSize;
            if ((b & 0x80) == 0)
            {
                break;
            }
        }

        return addrSize;
    }

    //! Processes a packet after header was received
    //! @param[in] lock The lock to use while processing the packet
    //! @return true if a full packet was processed
    //! @return false if not enough bytes received yet
    bool tryProcessCompletedPacket(LockGuard& lock)
    {
        if (mBuffer.size() < (kMinSizeAddress + kSizeCommand + kSizeCrc))
        {
            // Not enough data for address, command, and CRC.
            return false;
        }

        // Calculate CRC over message address, command, and payload (excluding CRC itself).
        const std::uint16_t calcCrc = computeCrc16(
            mBuffer.data(),
            mBuffer.size() - kSizeCrc
        );

        // Extract CRC from last 2 bytes.
        const std::uint16_t pktCrc = bytesToUint16(&mBuffer[mBuffer.size() - 2]);

        if (calcCrc != pktCrc)
        {
            // Done processing this packet.
            reset();
            return true;
        }

        const std::uint8_t addrSize = getAddressSize(mBuffer);
        if (mBuffer.size() < static_cast<std::size_t>(addrSize + kSizeCommand + kSizeCrc))
        {
            // Packet invalid: not enough data for address, command, and CRC.
            reset();
            return true;
        }

        // No need to process address into uint64 as it's not used internally
        std::string address(reinterpret_cast<const char*>(mBuffer.data()), addrSize);

        std::vector<std::uint8_t> packet = std::move(mBuffer);
        reset();

        // Now that buffers have been reset, process packet outside of lock context
        const bool releaseLock = lock.isLocked();
        if (releaseLock)
        {
            webusb_mutex->unlock();
        }

        processPkt(
            address,
            packet[addrSize],
            reinterpret_cast<const uint8_t*>(&packet[addrSize + kSizeCommand]),
            packet.size() - addrSize - kSizeCommand - kSizeCrc
        );

        if (releaseLock)
        {
            webusb_mutex->lock();
        }

        return true;
    }

    //! Process a received packet
    //! @param[in] address Address bytes
    //! @param[in] cmd Packet's command byte
    //! @param[in] payload Pointer to the beginning of the payload
    //! @param[in] payloadLen
    void processPkt(const std::string& address, const uint8_t cmd, const uint8_t* payload, uint16_t payloadLen)
    {
        std::unordered_map<std::uint8_t, std::shared_ptr<WebUsbCommandHandler>>::iterator iter = mParsers.find(cmd);
        if (iter != mParsers.end() && iter->second)
        {
            iter->second->process(
                payload,
                payloadLen,
                [this, address = address]
                (std::uint8_t responseCmd, const std::list<std::pair<const void*, std::uint16_t>>& payloadList) -> void
                {
                    sendPkt(address, responseCmd, payloadList);
                }
            );
        }
        else
        {
            // Unsupported command
            sendPkt(address, kCmdBadCmd, {{&cmd, sizeof(cmd)}});
        }
    }

    //! Perform vendor write to USB
    //! @param[in] itf Interface index to write to
    //! @param[in] buffer Buffer to write
    //! @param[in] bufsize The number of bytes to write
    //! @param[in] flush When true, perform flush
    //! @param[in] task When true, perform usb task before flush (ignored when flush is false)
    //! @return total number of bytes written
    static std::uint32_t vendorWrite(
        std::uint8_t itf,
        const void* buffer,
        std::uint32_t bufsize,
        bool flush = false,
        bool task = false
    )
    {
        const std::uint8_t* buffer8 = reinterpret_cast<const std::uint8_t*>(buffer);
        std::uint32_t consecutiveFailures = 0;
        std::uint32_t totalWritten = 0;
        while (bufsize > 0)
        {
            std::uint32_t written = tud_vendor_n_write(itf, buffer8, bufsize);

            if (written == 0 && bufsize > 0)
            {
                ++consecutiveFailures;
                if (consecutiveFailures >= 2)
                {
                    // Failed to write twice - return now
                    return totalWritten;
                }
            }
            else
            {
                consecutiveFailures = 0;
            }

            written = (written >= bufsize) ? bufsize : written;
            bufsize -= written;
            buffer8 += written;

            if (bufsize > 0 || flush)
            {
                if (task)
                {
                    tud_task();
                }
                tud_vendor_n_write_flush(itf);
            }

            totalWritten += written;
        }

        if (flush)
        {
            // Ensure final flush
            if (task)
            {
                tud_task();
            }
            tud_vendor_n_write_flush(itf);
        }

        return totalWritten;
    }

    //! Queues a packet for send which will actually be sent on next call to flushOutgoing()
    //! @param[in] address The outgoing address (same as the address received which triggered this response)
    //! @param[in] cmd The command to respond with
    //! @param[in] payloadList A list buffers to stitch together and send (will be copied by this call)
    //! @param[in] acquireLock True to acquire lock or false if lock was acquired by the caller
    void sendPkt(
        const std::string& address,
        const uint8_t cmd,
        const std::list<std::pair<const void*, std::uint16_t>>& payloadList,
        bool acquireLock = true
    )
    {
        std::uint16_t payloadLen = 0;
        for (const auto& it : payloadList)
        {
            payloadLen += it.second;
        }

        const std::uint16_t pktSize = address.size() + kSizeCommand + payloadLen + kSizeCrc;
        const std::uint16_t invPktSize = pktSize ^ 0xFFFF;
        std::uint8_t headerSize = static_cast<std::uint8_t>(kSizeMagic + kSizeSize + address.size() + kSizeCommand);
        std::uint8_t header[headerSize];
        memcpy(&header[0], k_webusb_magic_value, kSizeMagic);
        uint16ToBytes(&header[kSizeMagic], pktSize);
        uint16ToBytes(&header[kSizeMagic + sizeof(pktSize)], invPktSize);
        memcpy(&header[kSizeMagic + kSizeSize], address.data(), address.size());
        header[kSizeMagic + kSizeSize + address.size()] = cmd;

        // Calculate CRC over message address, command, and payload (excluding CRC itself)
        uint16_t crc = computeCrc16(&header[kSizeMagic + kSizeSize], headerSize - (kSizeMagic + kSizeSize));
        for (const auto& it : payloadList)
        {
            crc = computeCrc16(crc, it.first, it.second);
        }
        std::uint8_t crcBuffer[kSizeCrc];
        uint16ToBytes(crcBuffer, crc);

        // Build full packet into a single buffer and enqueue it. Actual TinyUSB writes
        // will be performed on core0 by calling `webusb_flush_outgoing()`.
        std::vector<uint8_t> fullPkt;
        fullPkt.reserve(headerSize + payloadLen + sizeof(crcBuffer));
        fullPkt.insert(fullPkt.end(), &header[0], &header[0] + headerSize);
        for (const auto& it : payloadList)
        {
            const uint8_t* p = reinterpret_cast<const uint8_t*>(it.first);
            fullPkt.insert(fullPkt.end(), p, p + it.second);
        }
        fullPkt.insert(fullPkt.end(), crcBuffer, crcBuffer + sizeof(crcBuffer));

        // Acquire lock, if necessary
        std::unique_ptr<LockGuard> optionalLock;
        if (acquireLock)
        {
            optionalLock = std::make_unique<LockGuard>(*webusb_mutex);
        }

        if (mOutgoingQueue.size() >= kMaxOutgoingSize)
        {
            // Throw out this packet
            return;
        }

        // Enqueue packet
        mLastSendSize = pktSize;
        mOutgoingQueue.push_back(std::move(fullPkt));
    }

    //! Converts 2 bytes from network order (big endian) to host order as uint16
    //! @param[in] payload Pointer to the data in the stream
    //! @return the converted uint16 value
    static std::uint16_t bytesToUint16(const void* payload)
    {
        const std::uint8_t* p8 = reinterpret_cast<const std::uint8_t*>(payload);
        return (static_cast<std::uint16_t>(p8[0]) << 8 | p8[1]);
    }

    //! Converts uint16 from host order to 2 bytes in network order (big endian)
    //! @param[out] out The buffer to write 2 bytes to
    //! @param[in] data The uint16 value to write
    static void uint16ToBytes(void* out, std::uint16_t data)
    {
        std::uint8_t* p8 = reinterpret_cast<std::uint8_t*>(out);
        *p8++ = data >> 8;
        *p8 = data & 0xFF;
    }

    //! Compute crc16 value from data
    //! @param[in] seed The input seed value; this should start with 0xFFFFU but may be crc of previous execution
    //! @param[in] buffer The buffer to read
    //! @param[in] bufLen Number of bytes in buffer to read
    //! @return a crc16 value
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

    //! Compute crc16 value from data
    //! @param[in] buffer The buffer to read
    //! @param[in] bufLen Number of bytes in buffer to read
    //! @return a crc16 value
    static std::uint16_t computeCrc16(const void* buffer, std::uint16_t bufLen)
    {
        return computeCrc16(0xFFFFU, buffer, bufLen);
    }

    //! Keep reading bytes until magic sequence is read or all bytes have been processed
    //! @param[in,out] buffer in: pointer to the beginning of the buffer to consume; out: incremented pointer
    //! @param[in,out] bufsize in: number of bytes to consume; out: number of bytes left to consume
    void parseMagic(const uint8_t*& buffer, std::size_t& bufsize)
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
    static std::unordered_map<std::uint8_t, std::shared_ptr<WebUsbCommandHandler>> mParsers;

    //! The USB vendor interface number
    const uint8_t mItf;

    //! Current receive index - this is negative while receiving size bytes
    std::int32_t mRcvIdx = -kSizeMagic;

    //! Received packet size
    std::uint8_t mSizeBytes[4] = {};

    //! Received size
    std::uint16_t mRcvSize = 0;

    //! Received data yet to be processed
    std::vector<std::uint8_t> mIncomingBuffer = std::vector<std::uint8_t>();

    //! Current receive buffer
    std::vector<std::uint8_t> mBuffer = std::vector<std::uint8_t>();

    //! Last send packet size or -1 if none
    std::int32_t mLastSendSize = -1;

    //! Queue of data waiting to be sent
    std::deque<std::vector<std::uint8_t>> mOutgoingQueue;
};

// definition of mParsers
std::unordered_map<std::uint8_t, std::shared_ptr<WebUsbCommandHandler>> WebUsbInterface::mParsers;

//! All available interfaces, mapped by interface number [0, CFG_TUD_VENDOR)
static std::vector<WebUsbInterface> webusb_interfaces = []() {
    std::vector<WebUsbInterface> arr;
    arr.reserve(CFG_TUD_VENDOR);
    for (uint8_t i = 0; i < CFG_TUD_VENDOR; ++i) {
        arr.push_back(WebUsbInterface(i));
    }
    return arr;
}();

void webusb_init(MutexInterface* mutex)
{
    webusb_mutex = mutex;
}

void webusb_connection_event(uint16_t interfaceNumber, uint16_t value)
{
    // Called from USB core (core 0)
    uint8_t index = ITF_TO_WEBUSB_IDX(interfaceNumber);
    if (index < webusb_interfaces.size() && value < 4)
    {
        // value
        // 0: Disconnect
        // 1: Connect and send zeros to shift out last command
        // 2: Connect and send null command
        // 3: Connect only

        // Connected or disconnected. In either case, clear write buffer.
        tud_vendor_n_write_clear(index);

        if (value != 0)
        {
            webusb_interfaces[index].connect(value == 1, value == 2);
        }
        else
        {
            webusb_interfaces[index].disconnect();
        }
    }
}

void webusb_rx(uint8_t itfIndex, const uint8_t* buffer, uint16_t bufsize)
{
    // Called from USB core (core 0)
    if (itfIndex < webusb_interfaces.size())
    {
        webusb_interfaces[itfIndex].addData(buffer, bufsize);
    }
}

void webusb_process()
{
    // Called from Maple core (core 1)
    for (WebUsbInterface& itf : webusb_interfaces)
    {
        itf.process();
    }
}

// Drain outgoing queue and perform TinyUSB writes on core0 only.
void webusb_flush_outgoing()
{
    for(WebUsbInterface& itf : webusb_interfaces)
    {
        itf.flushOutgoing();
    }
}

void webusb_add_parser(std::shared_ptr<WebUsbCommandHandler> parser)
{
    WebUsbInterface::addParser(parser);
}
