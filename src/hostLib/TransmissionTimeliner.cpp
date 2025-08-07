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

#include "TransmissionTimeliner.hpp"
#include <assert.h>
#include "configuration.h"

TransmissionTimeliner::TransmissionTimeliner(MapleBusInterface& bus, std::shared_ptr<PrioritizedTxScheduler> schedule):
    mBus(bus), mSchedule(schedule), mCurrentTx(nullptr)
{}

TransmissionTimeliner::ReadStatus TransmissionTimeliner::readTask(uint64_t currentTimeUs)
{
    ReadStatus status;

    // Process bus events and get any data received
    MapleBusInterface::Status busStatus = mBus.processEvents(currentTimeUs);
    status.busPhase = busStatus.phase;
    if (status.busPhase == MapleBusInterface::Phase::READ_COMPLETE)
    {
        status.received = std::make_shared<MaplePacket>(
            busStatus.readBuffer,
            busStatus.readBufferLen,
            busStatus.rxByteOrder
        );

        status.transmission = mCurrentTx;
        mCurrentTx = nullptr;
    }
    else if (status.busPhase == MapleBusInterface::Phase::WRITE_COMPLETE
             || status.busPhase == MapleBusInterface::Phase::READ_FAILED
             || status.busPhase == MapleBusInterface::Phase::WRITE_FAILED)
    {
        status.transmission = mCurrentTx;
        mCurrentTx = nullptr;
    }

    return status;
}

std::shared_ptr<const Transmission> TransmissionTimeliner::writeTask(uint64_t currentTimeUs)
{
    std::shared_ptr<const Transmission> txSent = nullptr;

    if (!mBus.isBusy())
    {
        PrioritizedTxScheduler::ScheduleItem item = mSchedule->peekNext(currentTimeUs);
        txSent = item.getTx();
        if (txSent != nullptr)
        {
            if (mBus.write(*txSent->packet, txSent->expectResponse, MAPLE_RESPONSE_TIMEOUT_US, txSent->rxByteOrder))
            {
                mCurrentTx = txSent;
                mSchedule->popItem(item);
            }
            else
            {
                txSent = nullptr;
            }
        }
    }

    return txSent;
}