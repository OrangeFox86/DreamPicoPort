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

#include "DreamcastController.hpp"
#include "dreamcast_constants.h"
#include <string.h>


DreamcastController::DreamcastController(uint8_t addr,
                                         uint32_t fd,
                                         std::shared_ptr<EndpointTxSchedulerInterface> scheduler,
                                         PlayerData playerData) :
    DreamcastPeripheral("controller", addr, fd, scheduler, playerData.playerIndex),
    mGamepad(playerData.gamepad),
    mWaitingForData(false),
    mFirstTask(true),
    mConditionTxId(0)
{
    mGamepad.controllerConnected();
}

DreamcastController::~DreamcastController()
{
    mGamepad.controllerDisconnected();
}

void DreamcastController::txStarted(std::shared_ptr<const Transmission> tx)
{
    if (mConditionTxId != 0 && tx->transmissionId == mConditionTxId)
    {
        mWaitingForData = true;
    }
}

void DreamcastController::txFailed(bool writeFailed,
                                   bool readFailed,
                                   std::shared_ptr<const Transmission> tx)
{
    if (mConditionTxId != 0 && tx->transmissionId == mConditionTxId)
    {
        mWaitingForData = false;
    }
}

void DreamcastController::txComplete(std::shared_ptr<const MaplePacket> packet,
                                     std::shared_ptr<const Transmission> tx)
{
    if (mWaitingForData && packet != nullptr)
    {
        mWaitingForData = false;

        if (packet->frame.command == COMMAND_RESPONSE_DATA_XFER
            && packet->payload.size() >= 3
            && packet->payload[0] == DEVICE_FN_CONTROLLER)
        {
            // Handle condition data
            DreamcastControllerObserver::ControllerCondition controllerCondition;
            memcpy(&controllerCondition, &packet->payload[1], 2 * sizeof(uint32_t));
            mGamepad.setControllerCondition(controllerCondition);
        }
    }
}

void DreamcastController::task(uint64_t currentTimeUs)
{
    if (mFirstTask)
    {
        mFirstTask = false;
        uint32_t payload[] = {DEVICE_FN_CONTROLLER};
        uint64_t txTime = PrioritizedTxScheduler::computeNextTimeCadence(currentTimeUs, US_PER_CHECK);
        mConditionTxId = mEndpointTxScheduler->add(
            EndpointTxSchedulerInterface::TransmissionProperties{
                .txTime = txTime,
                .command = COMMAND_GET_CONDITION,
                .payload = payload,
                .payloadLen = 1,
                .expectResponse = true,
                .expectedResponseNumPayloadWords = 3,
                .autoRepeatUs = US_PER_CHECK
            },
            this
        );
    }
}
