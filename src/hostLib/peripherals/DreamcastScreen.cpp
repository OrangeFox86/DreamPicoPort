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

#include "DreamcastScreen.hpp"
#include "dreamcast_constants.h"

DreamcastScreen::DreamcastScreen(
    uint8_t addr,
    uint32_t fd,
    const std::shared_ptr<EndpointTxSchedulerInterface>& scheduler,
    const std::shared_ptr<PlayerData>& playerData
) :
    DreamcastPeripheral("screen", addr, fd, scheduler, playerData->playerIndex),
    mNextCheckTime(0),
    mWaitingForData(false),
    mUpdateRequired(true),
    mScreenData(playerData->screenData),
    mTransmissionId(0)
{}

DreamcastScreen::~DreamcastScreen()
{}

void DreamcastScreen::txComplete(std::shared_ptr<const MaplePacket> packet,
                                 std::shared_ptr<const Transmission> tx)
{
    if (mWaitingForData && packet != nullptr)
    {
        mWaitingForData = false;
        mTransmissionId = 0;

        // TODO: return code is ignored for now; in the future, try to resend on failure
    }
}

void DreamcastScreen::task(uint64_t currentTimeUs)
{
    if (currentTimeUs > mNextCheckTime)
    {
        if (mScreenData->isNewDataAvailable() || mUpdateRequired)
        {
            // Write screen data
            static const uint8_t partitionNum = 0; // Always 0
            static const uint8_t sequenceNum = 0;  // 1 and only 1 in this sequence - always 0
            static const uint16_t blockNum = 0;    // Always 0
            static const uint32_t writeAddrWord = (partitionNum << 24) | (sequenceNum << 16) | blockNum;
            uint8_t numPayloadWords = ScreenData::NUM_SCREEN_WORDS + 2;
            uint32_t payload[numPayloadWords] = {DEVICE_FN_LCD, writeAddrWord, 0};
            mScreenData->readData(&payload[2]);

            if (mTransmissionId > 0 && !mWaitingForData)
            {
                // Make sure previous tx is canceled in case it hasn't gone out yet
                mEndpointTxScheduler->cancelById(mTransmissionId);
            }

            mTransmissionId = mEndpointTxScheduler->add(
                EndpointTxSchedulerInterface::TransmissionProperties{
                    .txTime = PrioritizedTxScheduler::TX_TIME_ASAP,
                    .command = COMMAND_BLOCK_WRITE,
                    .payload = payload,
                    .payloadLen = numPayloadWords,
                    .expectResponse = true,
                    .expectedResponseNumPayloadWords = 0
                },
                this
            );

            mNextCheckTime = currentTimeUs + US_PER_CHECK;

            mUpdateRequired = false;
        }
    }
}

void DreamcastScreen::txStarted(std::shared_ptr<const Transmission> tx)
{
    if (mTransmissionId > 0 && mTransmissionId == tx->transmissionId)
    {
        mWaitingForData = true;
    }
}

void DreamcastScreen::txFailed(bool writeFailed,
                               bool readFailed,
                               std::shared_ptr<const Transmission> tx)
{
    if (mTransmissionId > 0 && mTransmissionId == tx->transmissionId)
    {
        mWaitingForData = false;
        mTransmissionId = 0;
        // TODO: in the future, try to resend on failure
    }
}
