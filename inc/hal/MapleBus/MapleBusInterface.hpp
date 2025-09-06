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

#ifndef __MAPLE_BUS_INTERFACE_H__
#define __MAPLE_BUS_INTERFACE_H__

#include <stdint.h>
#include <memory>
#include "configuration.h"
#include "utils.h"
#include "MaplePacket.hpp"
#include <limits>

//! Maple Bus interface class
class MapleBusInterface
{
    public:
        //! Enumerates the phase in the state machine
        enum class Phase : uint8_t
        {
            //! Initialized phase and phase after completion and events are processed
            IDLE = 0,
            //! Write is currently in progress
            WRITE_IN_PROGRESS,
            //! Write has failed (impulse response used only as a result of processing events)
            WRITE_FAILED,
            //! Write completed and no read was expected
            WRITE_COMPLETE,
            //! Write completed, waiting for start sequence from device
            WAITING_FOR_READ_START,
            //! Currently waiting for response
            READ_IN_PROGRESS,
            //! Read has failed (impulse response used only as a result of processing events)
            READ_FAILED,
            //! Write and read cycle completed
            READ_COMPLETE,
            //! Initialized value
            INVALID
        };

        //! Enumerates different types of read/write errors
        enum class FailureReason : uint8_t
        {
            //! No error
            NONE = 0,
            //! CRC doesn't match computed value
            CRC_INVALID,
            //! Received less data than expected
            MISSING_DATA,
            //! Read DMA buffer overflowed
            BUFFER_OVERFLOW,
            //! Timeout occurred before data could be fully written or read
            TIMEOUT
        };

        //! Status due to processing events (see MapleBusInterface::processEvents)
        struct Status
        {
            //! The phase of the state machine
            Phase phase;
            //! Set to failure reason when phase is WRITE_FAILED or READ_FAILED
            FailureReason failureReason;
            //! A pointer to the bytes read or nullptr if no new data available
            const uint32_t* readBuffer;
            //! The number of words received or 0 if no new data available
            uint32_t readBufferLen;
            //! The byte order of the received packet
            MaplePacket::ByteOrder rxByteOrder;

            Status() :
                phase(Phase::INVALID),
                failureReason(FailureReason::NONE),
                readBuffer(nullptr),
                readBufferLen(0),
                rxByteOrder(MaplePacket::ByteOrder::HOST)
            {}
        };

    public:
        //! Virtual desturctor
        virtual ~MapleBusInterface() {}

        //! Writes a packet to the maple bus
        //! @post processEvents() must periodically be called to check status
        //! @param[in] packet  The packet to send (sender address will be overloaded)
        //! @param[in] autostartRead  Set to true in order to start receive after send is complete
        //! @param[in] readTimeoutUs  When autostartRead is true, the read timeout to set
        //! @param[in] rxBytePrder  When autostartRead is true, the desired byte order of the received packet
        //! @returns true iff the bus was "open" and send has started
        virtual bool write(
            const MaplePacket& packet,
            bool autostartRead,
            uint64_t readTimeoutUs=MAPLE_RESPONSE_TIMEOUT_US,
            MaplePacket::ByteOrder rxByteOrder = MaplePacket::ByteOrder::HOST
        ) = 0;

        //! Begins waiting for input
        //! @post processEvents() must periodically be called to check status
        //! @note This is NOT meant to be called if bus is setup as a host
        //! @note Keep in mind that the maple_in state machine doesn't  sample the full end
        //!       sequence. The application side should wait a sufficient amount of time after bus
        //!       goes neutral before responding in that case. Waiting for neutral bus within
        //!       write() may be enough though (as long as MAPLE_OPEN_LINE_CHECK_TIME_US is set to
        //!       at least 2).
        //! @param[in] readTimeoutUs  Minimum number of microseconds to read for (optional)
        //! @param[in] rxByteOrder  The desired byte order of the received packet
        //! @returns true iff bus was not busy and read started
        virtual bool startRead(
            uint64_t readTimeoutUs=std::numeric_limits<uint64_t>::max(),
            MaplePacket::ByteOrder rxByteOrder = MaplePacket::ByteOrder::HOST
        ) = 0;

        //! Processes timing events for the current time. This should be called before any write
        //! call in order to check timeouts and clear out any used resources.
        //! @param[in] currentTimeUs  The current time to process for
        //! @returns updated status since last call
        virtual Status processEvents(uint64_t currentTimeUs) = 0;

        //! @returns true iff the bus is currently busy reading or writing.
        virtual bool isBusy() = 0;
};

//! Creates a maple bus
//! @param[in] pinA  GPIO index for pin A. The very next GPIO will be designated as pin B.
extern std::shared_ptr<MapleBusInterface> create_maple_bus(uint32_t pinA, int32_t dirPin = -1, bool dirOutHigh = true);

#endif // __MAPLE_BUS_INTERFACE_H__
