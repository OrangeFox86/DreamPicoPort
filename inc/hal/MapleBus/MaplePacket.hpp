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

#ifndef __MAPLE_PACKET_H__
#define __MAPLE_PACKET_H__

#include <stdint.h>
#include <stddef.h>
#include <vector>
#include <utility>
#include "configuration.h"
#include "dreamcast_constants.h"

struct MaplePacket
{
    //! Defines the selected byte order of the packet
    enum class ByteOrder
    {
        //! Host: MSB is command
        HOST,
        //! Network: MSB is length
        NETWORK
    };

    //! Deconstructed frame word structure
    struct Frame
    {
        //! Command byte
        uint8_t command;
        //! Recipient address byte
        uint8_t recipientAddr;
        //! Sender address byte
        uint8_t senderAddr;
        //! Length of payload in words [0,255]
        uint8_t length;

        //! Byte position of the command in the frame word (host order)
        static const uint32_t COMMAND_POSITION_HOST = 24;
        //! Byte position of the recipient address in the frame word (host order)
        static const uint32_t RECIPIENT_ADDR_POSITION_HOST = 16;
        //! Byte position of the sender address in the frame word (host order)
        static const uint32_t SENDER_ADDR_POSITION_HOST = 8;
        //! Byte position of the payload length in the frame word (host order)
        static const uint32_t LEN_POSITION_HOST = 0;

        //! Byte position of the command in the frame word (network order)
        static const uint32_t COMMAND_POSITION_NETWORK = 0;
        //! Byte position of the recipient address in the frame word (network order)
        static const uint32_t RECIPIENT_ADDR_POSITION_NETWORK = 8;
        //! Byte position of the sender address in the frame word (network order)
        static const uint32_t SENDER_ADDR_POSITION_NETWORK = 16;
        //! Byte position of the payload length in the frame word (network order)
        static const uint32_t LEN_POSITION_NETWORK = 24;

        //! Set frame data from word
        inline void setFromFrameWord(uint32_t frameWord, ByteOrder byteOrder = ByteOrder::HOST)
        {
            length = getFramePacketLength(frameWord, byteOrder);
            senderAddr = getFrameSenderAddr(frameWord, byteOrder);
            recipientAddr = getFrameRecipientAddr(frameWord, byteOrder);
            command = getFrameCommand(frameWord, byteOrder);
        }

        //! Generate a default, invalid frame
        inline static Frame defaultFrame()
        {
            static const Frame f = {.command=COMMAND_INVALID};
            return f;
        }

        //! Generate a frame from a frame word
        inline static Frame fromWord(uint32_t frameWord, ByteOrder byteOrder = ByteOrder::HOST)
        {
            Frame f;
            f.setFromFrameWord(frameWord, byteOrder);
            return f;
        }

        //! @param[in] frameWord  The frame word to parse
        //! @returns the packet length specified in the given frame word
        static inline uint8_t getFramePacketLength(const uint32_t& frameWord, ByteOrder byteOrder = ByteOrder::HOST)
        {
            const bool isHostOrd = (byteOrder != ByteOrder::NETWORK);
            return ((frameWord >> (isHostOrd ? LEN_POSITION_HOST : LEN_POSITION_NETWORK )) & 0xFF);
        }

        //! @param[in] frameWord  The frame word to parse
        //! @returns the sender address specified in the given frame word
        static inline uint8_t getFrameSenderAddr(const uint32_t& frameWord, ByteOrder byteOrder = ByteOrder::HOST)
        {
            const bool isHostOrd = (byteOrder != ByteOrder::NETWORK);
            return ((frameWord >> (isHostOrd ? SENDER_ADDR_POSITION_HOST: SENDER_ADDR_POSITION_NETWORK)) & 0xFF);
        }

        //! @param[in] frameWord  The frame word to parse
        //! @returns the recipient address specified in the given frame word
        static inline uint8_t getFrameRecipientAddr(const uint32_t& frameWord, ByteOrder byteOrder = ByteOrder::HOST)
        {
            const bool isHostOrd = (byteOrder != ByteOrder::NETWORK);
            return ((frameWord >> (isHostOrd ? RECIPIENT_ADDR_POSITION_HOST : RECIPIENT_ADDR_POSITION_NETWORK)) & 0xFF);
        }

        //! @param[in] frameWord  The frame word to parse
        //! @returns the command specified in the given frame word
        static inline uint8_t getFrameCommand(const uint32_t& frameWord, ByteOrder byteOrder = ByteOrder::HOST)
        {
            const bool isHostOrd = (byteOrder != ByteOrder::NETWORK);
            return ((frameWord >> (isHostOrd ? COMMAND_POSITION_HOST : COMMAND_POSITION_NETWORK)) & 0xFF);
        }

        //! @returns the accumulated frame word from each of the frame data parts
        inline uint32_t toWord(ByteOrder byteOrder = ByteOrder::HOST) const
        {
            if (byteOrder == ByteOrder::NETWORK)
            {
                return (
                    static_cast<uint32_t>(length) << LEN_POSITION_NETWORK |
                    static_cast<uint32_t>(senderAddr) << SENDER_ADDR_POSITION_NETWORK |
                    static_cast<uint32_t>(recipientAddr) << RECIPIENT_ADDR_POSITION_NETWORK |
                    static_cast<uint32_t>(command) << COMMAND_POSITION_NETWORK
                );
            }
            else
            {
                return (
                    static_cast<uint32_t>(length) << LEN_POSITION_HOST |
                    static_cast<uint32_t>(senderAddr) << SENDER_ADDR_POSITION_HOST |
                    static_cast<uint32_t>(recipientAddr) << RECIPIENT_ADDR_POSITION_HOST |
                    static_cast<uint32_t>(command) << COMMAND_POSITION_HOST
                );
            }
        }

        //! Assignment operator
        Frame& operator=(const Frame& rhs)
        {
            length = rhs.length;
            senderAddr = rhs.senderAddr;
            recipientAddr = rhs.recipientAddr;
            command = rhs.command;
            return *this;
        }

        //! == operator for this class
        inline bool operator==(const Frame& rhs) const
        {
            return (
                length == rhs.length
                && senderAddr == rhs.senderAddr
                && recipientAddr == rhs.recipientAddr
                && command == rhs.command
            );
        }

        //! @returns true iff frame word is valid
        inline bool isValid() const
        {
            return (command != COMMAND_INVALID);
        }
    };

    //! Constructor 1
    //! @param[in] frame  Frame data to initialize
    //! @param[in] payload  The payload words to set
    //! @param[in] len  Number of words in payload
    inline MaplePacket(Frame frame, const uint32_t* payload, uint8_t len, ByteOrder byteOrder = ByteOrder::HOST) :
        frame(frame),
        payload(payload, payload + len),
        payloadByteOrder(byteOrder)
    {
        updateFrameLength();
    }

    //! Constructor 2 (default) - initializes with invalid packet
    inline MaplePacket() :
        MaplePacket(Frame::defaultFrame(), NULL, 0)
    {}

    //! Constructor 3 - initializes with empty payload
    //! @param[in] frame  Frame data to initialize
    inline MaplePacket(Frame frame) :
        MaplePacket(frame, NULL, 0)
    {}

    //! Constructor 4 - initializes with frame and 1 payload word
    //! @param[in] frame  Frame data to initialize
    //! @param[in] payload  The single payload word to set
    inline MaplePacket(Frame frame, uint32_t payload, ByteOrder byteOrder = ByteOrder::HOST) :
        MaplePacket(frame, &payload, 1, byteOrder)
    {}

    //! Constructor 5
    //! @param[in] words  All words to set
    //! @param[in] len  Number of words in words (must be at least 1 for frame word to be valid)
    inline MaplePacket(const uint32_t* words, uint8_t len, ByteOrder byteOrder = ByteOrder::HOST) :
        MaplePacket(
            len > 0 ? Frame::fromWord(*words, byteOrder) : Frame::defaultFrame(),
            words + 1,
            len > 0 ? len - 1 : 0,
            byteOrder
        )
    {}

    //! Constructor 6 - initializes from payload and moved vector
    //! @param[in] payload  The payload
    inline MaplePacket(Frame frame, std::vector<uint32_t>&& words, ByteOrder byteOrder = ByteOrder::HOST) :
        frame(frame),
        payload(std::move(words)),
        payloadByteOrder(byteOrder)
    {
        updateFrameLength();
    }

    //! Copy constructor
    inline MaplePacket(const MaplePacket& rhs) :
        frame(rhs.frame),
        payload(rhs.payload),
        payloadByteOrder(rhs.payloadByteOrder)
    {}

    //! Move constructor
    inline MaplePacket(MaplePacket&& rhs) noexcept :
        frame(rhs.frame),
        payload(std::move(rhs.payload)),
        payloadByteOrder(rhs.payloadByteOrder)
    {}

    //! Assignment operator
    MaplePacket& operator=(const MaplePacket& rhs)
    {
        frame = rhs.frame;
        payload = rhs.payload;
        payloadByteOrder = rhs.payloadByteOrder;
        return *this;
    }

    //! == operator for this class
    inline bool operator==(const MaplePacket& rhs) const
    {
        return (
            frame == rhs.frame &&
            payload == rhs.payload &&
            payloadByteOrder == rhs.payloadByteOrder
        );
    }

    //! @returns frame word value with corrected length in the payload byte order
    uint32_t getFrameWord() const
    {
        Frame f = frame;
        f.length = payload.size();
        return f.toWord(payloadByteOrder);
    }

    //! Resets all data
    inline void reset()
    {
        frame = Frame::defaultFrame();
        payload.clear();
        updateFrameLength();
    }

    //! Reserves space in payload
    //! @param[in] len  Number of words to reserve
    inline void reservePayload(uint32_t len)
    {
        payload.reserve(len);
    }

    //! Sets packet contents from array
    //! @param[in] words  All words to set
    //! @param[in] len  Number of words in words (must be at least 1 for frame word to be valid)
    inline void set(const uint32_t* words, uint8_t len, ByteOrder byteOrder = ByteOrder::HOST)
    {
        payloadByteOrder = byteOrder;
        if (len > 0)
        {
            frame.setFromFrameWord(words[0], byteOrder);
        }
        else
        {
            frame = Frame::defaultFrame();
        }
        payload.clear();
        if (len > 1)
        {
            payload.insert(payload.end(), &words[1], &words[1] + (len - 1));
        }
        updateFrameLength();
    }

    //! Append words to payload from array
    //! @param[in] words  Payload words to set
    //! @param[in] len  Number of words in words
    inline void appendPayload(const uint32_t* words, uint8_t len, ByteOrder byteOrder = ByteOrder::HOST)
    {
        if (len > 0)
        {
            bool flipWords = ((payloadByteOrder == ByteOrder::NETWORK) != (byteOrder == ByteOrder::NETWORK));
            if (flipWords)
            {
                reservePayload(payload.size() + len);
                while (len-- > 0)
                {
                    payload.push_back(flipWordBytes(*words++));
                }
            }
            else
            {
                payload.insert(payload.end(), &words[0], &words[0] + len);
            }

            updateFrameLength();
        }
    }

    //! Appends a single word to payload
    //! @param[in] word  The word to append
    inline void appendPayload(uint32_t word, ByteOrder byteOrder = ByteOrder::HOST)
    {
        appendPayload(&word, 1, byteOrder);
    }

    //! Sets payload from array
    //! @param[in] words  Payload words to set
    //! @param[in] len  Number of words in words
    inline void setPayload(const uint32_t* words, uint8_t len, ByteOrder byteOrder = ByteOrder::HOST)
    {
        payload.clear();
        payloadByteOrder = byteOrder;
        appendPayload(words, len, byteOrder);
    }

    //! Sets a single word in payload
    //! @param[in] word  The word to set
    inline void setPayload(uint32_t word, ByteOrder byteOrder = ByteOrder::HOST)
    {
        setPayload(&word, 1, byteOrder);
    }

    //! Update length in frame word with the payload size
    void updateFrameLength()
    {
        frame.length = payload.size();
    }

    //! @returns true iff frame word is valid
    inline bool isValid() const
    {
        return (frame.isValid() && frame.length == payload.size());
    }

    //! @param[in] numPayloadWords  Number of payload words in the packet
    //! @returns number of bits that a packet makes up
    inline static uint32_t getNumTotalBits(uint32_t numPayloadWords)
    {
        // payload size + frame word size + crc byte
        return (((numPayloadWords + 1) * 4 + 1) * 8);
    }

    //! @returns number of bits that this packet makes up
    inline uint32_t getNumTotalBits() const
    {
        return getNumTotalBits(payload.size());
    }

    //! @param[in] numPayloadWords  Number of payload words in the packet
    //! @param[in] nsPerBit  Nanoseconds to transmit each bit
    //! @returns number of nanoseconds it takes to transmit a packet
    inline static uint32_t getTxTimeNs(uint32_t numPayloadWords, uint32_t nsPerBit)
    {
        // Start and stop sequence takes less than 14 bit periods
        return (getNumTotalBits(numPayloadWords) + 14) * nsPerBit;
    }

    //! @returns number of nanoseconds it takes to transmit this packet
    inline uint32_t getTxTimeNs() const
    {
        return getTxTimeNs(payload.size(), MAPLE_NS_PER_BIT);
    }

    static uint32_t flipWordBytes(const uint32_t& word)
    {
        return (word << 24) | (word << 8 & 0xFF0000) | (word >> 8 & 0xFF00) | (word >> 24);
    }

    //! Packet frame word value
    Frame frame;
    //! Packet payload
    std::vector<uint32_t> payload;
    //! The byte order of the payload
    ByteOrder payloadByteOrder;
};

#endif // __MAPLE_PACKET_H__
