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

#include "hal/MapleBus/MaplePacket.hpp"
#include "hal/MapleBus/MapleBusInterface.hpp"
#include "PrioritizedTxScheduler.hpp"

class TransmissionTimeliner
{
public:
    //! Status of the task
    struct ReadStatus
    {
        //! The transmission associated with the data below
        std::shared_ptr<const Transmission> transmission;
        //! Set to received packet or nullptr if nothing received
        std::shared_ptr<const MaplePacket> received;
        //! The phase of the maple bus
        MapleBusInterface::Phase busPhase;

        ReadStatus() :
            transmission(nullptr),
            received(nullptr),
            busPhase(MapleBusInterface::Phase::INVALID)
        {}
    };

public:
    //! Constructor
    //! @param[in] bus  The maple bus that scheduled transmissions are written to
    //! @param[in] schedule  The schedule to pop transmissions from
    TransmissionTimeliner(MapleBusInterface& bus, std::shared_ptr<PrioritizedTxScheduler> schedule);

    //! Read timeliner task - called periodically to process timeliner read events
    //! @param[in] currentTimeUs  The current time task is run
    //! @returns read status information
    ReadStatus readTask(uint64_t currentTimeUs);

    //! Write timeliner task - called periodically to process timeliner write events
    //! @param[in] currentTimeUs  The current time task is run
    //! @returns the transmission that started or nullptr if nothing was transmitted
    std::shared_ptr<const Transmission> writeTask(uint64_t currentTimeUs);

protected:
    //! The maple bus that scheduled transmissions are written to
    MapleBusInterface& mBus;
    //! The schedule that transmissions are popped from
    std::shared_ptr<PrioritizedTxScheduler> mSchedule;
    //! The currently sending transmission
    std::shared_ptr<const Transmission> mCurrentTx;
};