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

#include "DreamcastSubNode.hpp"
#include "dreamcast_constants.h"
#include "utils.h"

DreamcastSubNode::DreamcastSubNode(
    uint8_t addr,
    std::shared_ptr<EndpointTxSchedulerInterface> scheduler,
    const std::shared_ptr<PlayerData>& playerData
) :
    DreamcastNode(addr, scheduler, playerData),
    mConnected(false),
    mScheduleId(-1)
{
}

DreamcastSubNode::DreamcastSubNode(const DreamcastSubNode& rhs) :
    DreamcastNode(rhs),
    mConnected(rhs.mConnected),
    mScheduleId(rhs.mScheduleId)
{
}

void DreamcastSubNode::txComplete(std::shared_ptr<const MaplePacket> packet,
                                  std::shared_ptr<const Transmission> tx)
{
    // If device info received, add the sub peripheral
    if (packet->frame.command == COMMAND_RESPONSE_DEVICE_INFO)
    {
        if (packet->payload.size() > 3)
        {
            uint32_t mask = peripheralFactory(packet->payload);
            if (mPeripherals.size() > 0)
            {
                DEBUG_PRINT("P%lu-%li connected (",
                            mPlayerData.playerIndex + 1,
                            DreamcastPeripheral::subPeripheralIndex(mAddr) + 1);
                debugPrintPeripherals();
                DEBUG_PRINT(")\n");
            }

            if (mask > 0)
            {
                DEBUG_PRINT("P%lu-%li unknown device(s) in mask: 0x%08lx\n",
                            mPlayerData.playerIndex + 1,
                            DreamcastPeripheral::subPeripheralIndex(mAddr) + 1,
                            mask);
            }

            // Remove the auto reload device info request transmission from schedule
            // This is done even if no known peripheral detected
            if (mScheduleId >= 0)
            {
                mEndpointTxScheduler->cancelById(mScheduleId);
                mScheduleId = -1;
            }
        }
    }
}

void DreamcastSubNode::task(uint64_t currentTimeUs)
{
    if (mConnected)
    {
        // Handle operations for peripherals (run task() of all peripherals)
        if (mPeripherals.size() > 0)
        {
            // Have the connected main peripheral handle write
            handlePeripherals(currentTimeUs);
        }
    }
}

void DreamcastSubNode::mainPeripheralDisconnected()
{
    setConnected(false);
}

bool DreamcastSubNode::setConnected(bool connected, uint64_t currentTimeUs)
{
    if (mConnected != connected)
    {
        mConnected = connected;
        mPeripherals.clear();
        mEndpointTxScheduler->cancelByRecipient(getRecipientAddress());
        if (mConnected)
        {
            // Keep asking for info until valid response is heard
            uint64_t txTime = PrioritizedTxScheduler::TX_TIME_ASAP;
            if (currentTimeUs > 0)
            {
                txTime = PrioritizedTxScheduler::computeNextTimeCadence(currentTimeUs, US_PER_CHECK);
            }
            mScheduleId = mEndpointTxScheduler->add(
                EndpointTxSchedulerInterface::TransmissionProperties{
                    .txTime = txTime,
                    .command = COMMAND_DEVICE_INFO_REQUEST,
                    .payload = nullptr,
                    .payloadLen = 0,
                    .expectResponse = true,
                    .expectedResponseNumPayloadWords = EXPECTED_DEVICE_INFO_PAYLOAD_WORDS,
                    .autoRepeatUs = US_PER_CHECK
                },
                this
            );
        }
        else
        {
            DEBUG_PRINT("P%lu-%li disconnected\n",
                        mPlayerData.playerIndex + 1,
                        DreamcastPeripheral::subPeripheralIndex(mAddr) + 1);
        }

        return true;
    }

    return false;
}