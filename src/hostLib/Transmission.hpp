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

#pragma once

#include <stdint.h>
#include <memory>
#include "hal/MapleBus/MaplePacket.hpp"
#include "Transmitter.hpp"

//! Transmission definition
struct Transmission
{
public:
    //! Unique ID of this transmission
    const uint32_t transmissionId;
    //! Priority where 0 is highest
    const uint8_t priority;
    //! Set to true iff a response is expected
    const bool expectResponse;
    //! The expected transmission duration (from transmit to end of receive)
    const uint32_t txDurationUs;
    //! If not 0, the period which this transmission should be repeated in microseconds
    const uint32_t autoRepeatUs;
    //! If not 0, auto repeat will cancel after this time
    const uint64_t autoRepeatEndTimeUs;
    //! The next time that this packet is to be transmitted
    uint64_t nextTxTimeUs;
    //! The packet to transmit
    std::shared_ptr<const MaplePacket> packet;
    //! When expectResponse is true, the desired byte order of the received packet
    MaplePacket::ByteOrder rxByteOrder;
    //! The object that added this transmission (for callbacks)
    Transmitter* const transmitter;
    //! The object that added this transmission (for callbacks, shared pointer version)
    const std::shared_ptr<Transmitter> spTransmitter;

public:
    Transmission(
        uint32_t transmissionId,
        uint8_t priority,
        bool expectResponse,
        uint32_t txDurationUs,
        uint32_t autoRepeatUs,
        uint64_t autoRepeatEndTimeUs,
        uint64_t nextTxTimeUs,
        std::shared_ptr<MaplePacket> packet,
        MaplePacket::ByteOrder rxByteOrder,
        Transmitter* transmitter
    ):
        transmissionId(transmissionId),
        priority(priority),
        expectResponse(expectResponse),
        txDurationUs(txDurationUs),
        autoRepeatUs(autoRepeatUs),
        autoRepeatEndTimeUs(autoRepeatEndTimeUs),
        nextTxTimeUs(nextTxTimeUs),
        packet(packet),
        rxByteOrder(rxByteOrder),
        transmitter(transmitter),
        spTransmitter()
    {}

    Transmission(
        uint32_t transmissionId,
        uint8_t priority,
        bool expectResponse,
        uint32_t txDurationUs,
        uint32_t autoRepeatUs,
        uint64_t autoRepeatEndTimeUs,
        uint64_t nextTxTimeUs,
        std::shared_ptr<MaplePacket> packet,
        MaplePacket::ByteOrder rxByteOrder,
        const std::shared_ptr<Transmitter>& transmitter
    ):
        transmissionId(transmissionId),
        priority(priority),
        expectResponse(expectResponse),
        txDurationUs(txDurationUs),
        autoRepeatUs(autoRepeatUs),
        autoRepeatEndTimeUs(autoRepeatEndTimeUs),
        nextTxTimeUs(nextTxTimeUs),
        packet(packet),
        rxByteOrder(rxByteOrder),
        transmitter(),
        spTransmitter(transmitter)
    {}

    //! @returns the estimated completion time of this transmission
    uint64_t getNextCompletionTime(uint64_t executionTime)
    {
        return executionTime + txDurationUs;
    }
};