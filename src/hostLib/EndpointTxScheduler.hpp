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

#include "EndpointTxSchedulerInterface.hpp"
#include "hal/MapleBus/MaplePacket.hpp"
#include "dreamcast_constants.h"
#include "PrioritizedTxScheduler.hpp"
#include <memory>

class EndpointTxScheduler : public EndpointTxSchedulerInterface
{
public:
    //! Default constructor
    EndpointTxScheduler(std::shared_ptr<PrioritizedTxScheduler> prioritizedScheduler,
                        uint8_t fixedPriority,
                        uint8_t recipientAddr);

    //! Virtual destructor
    virtual ~EndpointTxScheduler();

    //! Add a transmission to the schedule
    //! @param[in] properties  The selected properties for this transmission
    //! @param[in] transmitter  Pointer to transmitter that is adding this
    //! @returns transmission ID
    virtual uint32_t add(
        TransmissionProperties properties,
        Transmitter* transmitter
    ) override final;

    //! Add a transmission to the schedule
    //! @param[in] properties  The selected properties for this transmission
    //! @param[in] transmitter  Pointer to transmitter that is adding this (kept alive until transmission completes)
    //! @returns transmission ID
    virtual uint32_t add(
        TransmissionProperties properties,
        const std::shared_ptr<Transmitter>& transmitter
    ) override final;

    //! Cancels scheduled transmission by transmission ID
    //! @param[in] transmissionId  The transmission ID of the transmissions to cancel
    //! @returns number of transmissions successfully canceled
    virtual uint32_t cancelById(uint32_t transmissionId) override final;

    //! Cancels scheduled transmission by recipient address
    //! @param[in] recipientAddr  The recipient address of the transmissions to cancel
    //! @returns number of transmissions successfully canceled
    virtual uint32_t cancelByRecipient(uint8_t recipientAddr) override final;

    //! Count how many scheduled transmissions have a given recipient address
    //! @param[in] recipientAddr  The recipient address
    //! @returns the number of transmissions have the given recipient address
    virtual uint32_t countRecipients(uint8_t recipientAddr) override final;

    //! Cancels all items in the schedule
    //! @returns number of transmissions successfully canceled
    virtual uint32_t cancelAll() final;

private:
    std::shared_ptr<PrioritizedTxScheduler> mPrioritizedScheduler;
    uint8_t mFixedPriority;
    uint8_t mRecipientAddr;
};