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

#include "DreamcastTimer.hpp"
#include <memory.h>

DreamcastTimer::DreamcastTimer(uint8_t addr,
                               uint32_t fd,
                               std::shared_ptr<EndpointTxSchedulerInterface> scheduler,
                               PlayerData playerData) :
    DreamcastPeripheral("timer", addr, fd, scheduler, playerData.playerIndex),
    mGamepad(playerData.gamepad),
    mButtonStatusId(0)
{
    // Poll only the upper VMU button states
    if (addr & SUB_PERIPHERAL_ADDR_START_MASK)
    {
        uint32_t payload = FUNCTION_CODE;
        mButtonStatusId = mEndpointTxScheduler->add(
            EndpointTxSchedulerInterface::TransmissionProperties{
                .txTime = PrioritizedTxScheduler::TX_TIME_ASAP,
                .command = COMMAND_GET_CONDITION,
                .payload = &payload,
                .payloadLen = 1,
                .expectResponse = true,
                .expectedResponseNumPayloadWords = 2,
                .autoRepeatUs = BUTTON_POLL_PERIOD_US
            },
            this
        );
    }
}

DreamcastTimer::~DreamcastTimer()
{}

void DreamcastTimer::task(uint64_t currentTimeUs)
{}

void DreamcastTimer::txStarted(std::shared_ptr<const Transmission> tx)
{}

void DreamcastTimer::txFailed(bool writeFailed,
                              bool readFailed,
                              std::shared_ptr<const Transmission> tx)
{}

void DreamcastTimer::txComplete(std::shared_ptr<const MaplePacket> packet,
                                std::shared_ptr<const Transmission> tx)
{
    if (tx->transmissionId == mButtonStatusId
        && packet->frame.command == COMMAND_RESPONSE_DATA_XFER
        && packet->payload.size() >= 2)
    {
        // Set controller!
        const uint8_t cond = packet->payload[1] >> COND_RIGHT_SHIFT;
        DreamcastControllerObserver::SecondaryControllerCondition secondaryCondition;
        memcpy(&secondaryCondition, &cond, 1);
        mGamepad.setSecondaryControllerCondition(secondaryCondition);
    }
}
