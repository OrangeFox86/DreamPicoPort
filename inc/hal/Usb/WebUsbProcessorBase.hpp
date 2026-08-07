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

#include "hal/System/LockGuard.hpp"
#include "WebUsbCommandHandler.hpp"
#include "utils.h"

#include <cstring>
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


//! The magic value that every packet must begin with
static constexpr std::uint8_t k_webusb_magic_value[] = {0xDB, 0x8B, 0xAF, 0xD5};

//! This is a base class for a WebUsb processor on a DreamPicoPort. This is provided mainly to be able to unit test the
//! processor outside of tinyusb.
//!
//! Packet format (big endian order):
//! Magic Bytes [4] | Size [2] | Inverse Size [2] | Return Address [1-9] | Command [1] | Payload [0-N] | CRC [2]
//! Size is number of bytes in return address, command, payload, and crc
//! Return address must keep growing until most significant bit is 0, 9 bytes are read, or size has elapsed
//! CRC covers return address, command, and payload
//!
//! Note: the purpose of Return Address is to tag each command so the host knows where to direct the response without
//!       the need to serialize communication. The address may be up to 64 bits in size.
class WebUsbProcessorBase
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
    WebUsbProcessorBase() = delete;

    //! Constructor
    //! @param[in] itf The interface index
    WebUsbProcessorBase(uint8_t itf) : mItf(itf) {}

    //! Perform runtime initialization
    //! @param[in] mutex Pointer to a static mutex to store internally
    void init(MutexInterface* mutex)
    {
        mWebusbMutex = mutex;
    }

    //! Fully reset due to external event
    //! @param[in] sendZeros When true, send at least 256 zeros or number of zeros equivalent to last send size.
    //!                      This ensures that anything stuck in the USB output buffer is fully purged.
    //! @param[in] sendNullPkt When true, sends a "null packet" with command of 0 and maximum address value.
    //!                        The host may use this to synchronize to the beginning of the stream.
    void externalReset(bool sendZeros = false, bool sendNullPkt = false)
    {
        LockGuard lock(*mWebusbMutex);
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
                vendorWrite(buff, s, false);
                writeSize -= s;
            }
            // to ensure crc gets processed too
            vendorWrite(buff, 16, true);
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

    //! Add data from the incoming stream
    //! @param[in] buffer Pointer to the stream
    //! @param[in] bufsize Number of bytes to read
    void addData(const uint8_t* buffer, uint16_t bufsize)
    {
        if (bufsize > 0)
        {
            LockGuard lock(*mWebusbMutex);

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
        LockGuard lock(*mWebusbMutex);

        if (!mIncomingBuffer.empty())
        {
            newData = std::move(mIncomingBuffer);
            mIncomingBuffer.clear();
        }

        const uint8_t* buffer = newData.data();
        const uint8_t* const bufferEnd = newData.data() + newData.size();

        while (buffer < bufferEnd)
        {
            if (!tryReadPacketHeader(buffer, bufferEnd))
            {
                // Consumed entire buffer without completing packet header.
                return;
            }

            consumePacketBytes(buffer, bufferEnd);

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
                LockGuard lock(*mWebusbMutex);
                if (mOutgoingQueue.empty())
                {
                    return;
                }
                pkt = mOutgoingQueue.front();
                mOutgoingQueue.pop_front();
            }

            vendorWrite(pkt.data(), static_cast<uint32_t>(pkt.size()), true, true);
        }
    }

protected:
    //! Consume bytes until all data is consumed or until full header is read (magic + size bytes)
    //! @param[in,out] buffer in: pointer to the beginning of the buffer to consume; out: incremented pointer
    //! @param[in] bufferEnd Pointer to the end of the buffer
    //! @return true if a full header is received
    //! @return false if bufsize is 0 and a full header has not been received yet
    bool tryReadPacketHeader(const uint8_t*& buffer, const uint8_t* const bufferEnd)
    {
        while (mRcvIdx < kSizeSize)
        {
            parseMagic(buffer, bufferEnd);

            while (mRcvIdx < kSizeSize && buffer < bufferEnd)
            {
                mSizeBytes[mRcvIdx] = *buffer;

                ++mRcvIdx;
                ++buffer;
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
                const uint8_t* const tmpBufferEnd = tmpBuffer + sizeof(mSizeBytes);
                parseMagic(tmpBuffer, tmpBufferEnd);
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
    //! @param[in] bufferEnd Pointer to the end of the buffer
    void consumePacketBytes(const uint8_t*& buffer, const uint8_t* const bufferEnd)
    {
        // mRcvIdx is guaranteed to be >= kSizeSize here.
        const std::uint16_t payloadIdx = mRcvIdx - kSizeSize;
        std::uint16_t bytesToConsume = mRcvSize - payloadIdx;
        const std::size_t bufsize = (bufferEnd > buffer) ? (bufferEnd - buffer) : 0;
        if (bufsize < bytesToConsume)
        {
            bytesToConsume = bufsize;
        }

        mBuffer.insert(mBuffer.end(), buffer, buffer + bytesToConsume);

        mRcvIdx += bytesToConsume;
        buffer += bytesToConsume;
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
            mWebusbMutex->unlock();
        }

        processPkt(
            address,
            packet[addrSize],
            reinterpret_cast<const uint8_t*>(&packet[addrSize + kSizeCommand]),
            packet.size() - addrSize - kSizeCommand - kSizeCrc
        );

        if (releaseLock)
        {
            mWebusbMutex->lock();
        }

        return true;
    }

    //! Process a received packet
    //! @param[in] address Address bytes
    //! @param[in] cmd Packet's command byte
    //! @param[in] payload Pointer to the beginning of the payload
    //! @param[in] payloadLen
    virtual void processPkt(
        const std::string& address,
        const uint8_t cmd,
        const uint8_t* payload,
        uint16_t payloadLen
    ) = 0;

    //! Perform vendor write to USB
    //! @param[in] itf Interface index to write to
    //! @param[in] buffer Buffer to write
    //! @param[in] bufsize The number of bytes to write
    //! @param[in] flush When true, perform flush
    //! @param[in] task When true, perform usb task before flush (ignored when flush is false)
    //! @return total number of bytes written
    virtual std::uint32_t vendorWrite(
        const void* buffer,
        std::uint32_t bufsize,
        bool flush = false,
        bool task = false
    ) = 0;

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
            optionalLock = std::make_unique<LockGuard>(*mWebusbMutex);
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
    //! @param[in] bufferEnd Pointer to the end of the buffer
    void parseMagic(const uint8_t*& buffer, const uint8_t* const bufferEnd)
    {
        while (mRcvIdx < 0 && buffer < bufferEnd)
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
        }
    }

public:
    //! The USB vendor interface number
    const uint8_t mItf;

private:
    //! Mutex ensuring serialization to webusb output
    MutexInterface* mWebusbMutex = nullptr;

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
