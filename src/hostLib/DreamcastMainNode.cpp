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

#include "DreamcastMainNode.hpp"
#include "DreamcastPeripheral.hpp"
#include "dreamcast_constants.h"
#include "DreamcastController.hpp"
#include "EndpointTxScheduler.hpp"

DreamcastMainNode::DreamcastMainNode(
    const std::shared_ptr<MapleBusInterface>& bus,
    const std::shared_ptr<PlayerData>& playerData,
    const std::shared_ptr<PrioritizedTxScheduler>& prioritizedTxScheduler,
    bool detectionOnly
) :
    DreamcastNode(
        DreamcastPeripheral::MAIN_PERIPHERAL_ADDR_MASK,
        std::make_shared<EndpointTxScheduler>(
            prioritizedTxScheduler,
            PrioritizedTxScheduler::MAIN_TRANSMISSION_PRIORITY,
            DreamcastPeripheral::getRecipientAddress(
                playerData->playerIndex,
                DreamcastPeripheral::MAIN_PERIPHERAL_ADDR_MASK
            )
        ),
        playerData
    ),
    mDetectionOnly(detectionOnly),
    mDeviceDetected(false),
    mSubNodes(),
    mTransmissionTimeliner(bus, prioritizedTxScheduler),
    mScheduleId(-1),
    mCommFailCount(0),
    mPrintSummary(false),
    mSendConnectedSignal(false),
    mChangeReleaseTime(0)
{
    addInfoRequestToSchedule();
    mSubNodes.reserve(DreamcastPeripheral::MAX_SUB_PERIPHERALS);
    for (uint32_t i = 0; i < DreamcastPeripheral::MAX_SUB_PERIPHERALS; ++i)
    {
        uint8_t addr = DreamcastPeripheral::subPeripheralMask(i);
        mSubNodes.push_back(std::make_shared<DreamcastSubNode>(
            addr,
            std::make_shared<EndpointTxScheduler>(
                prioritizedTxScheduler,
                PrioritizedTxScheduler::SUB_TRANSMISSION_PRIORITY,
                DreamcastPeripheral::getRecipientAddress(playerData->playerIndex, addr)),
            mPlayerData));
    }
}

DreamcastMainNode::~DreamcastMainNode()
{}

void DreamcastMainNode::txComplete(
    std::shared_ptr<const MaplePacket> packet,
    std::shared_ptr<const Transmission> tx
)
{
    // Handle device info from main peripheral
    if (packet != nullptr && packet->frame.command == COMMAND_RESPONSE_DEVICE_INFO)
    {
        mDeviceDetected = true;

        if (mDetectionOnly)
        {
            // This node will now be completely idle
            cancelInfoRequest();
        }
        else if (packet->payload.size() > 3)
        {
            uint32_t mask = peripheralFactory(packet->payload);
            if (mPeripherals.size() > 0)
            {
                // Remove the auto reload device info request transmission from schedule
                cancelInfoRequest();

                DEBUG_PRINT("P%lu connected (", mPlayerData.playerIndex + 1);
                debugPrintPeripherals();
                DEBUG_PRINT(")\n");

                mSendConnectedSignal = true;

                // Reset failure count
                mCommFailCount = 0;
            }

            if (mask > 0)
            {
                DEBUG_PRINT("P%lu unknown device(s) in mask: 0x%08lx\n",
                            mPlayerData.playerIndex + 1,
                            mask);
            }
        }
    }
}

void DreamcastMainNode::disconnectMainPeripheral(uint64_t currentTimeUs)
{
    mDeviceDetected = false;
    peripheralChangeEvent(currentTimeUs);
    mPeripherals.clear();
    mEndpointTxScheduler->cancelByRecipient(getRecipientAddress());
    for (std::vector<std::shared_ptr<DreamcastSubNode>>::iterator iter = mSubNodes.begin();
            iter != mSubNodes.end();
            ++iter)
    {
        (*iter)->mainPeripheralDisconnected();
    }
    addInfoRequestToSchedule(currentTimeUs);
    DEBUG_PRINT("P%lu disconnected\n", mPlayerData.playerIndex + 1);
}

void DreamcastMainNode::printSummary()
{
    mPrintSummary = true;
}

void DreamcastMainNode::requestSummary(
    const std::function<void(const std::list<std::list<std::array<uint32_t, 2>>>&)>& callback)
{
    mSummaryCallback = callback;
}

void DreamcastMainNode::readTask(uint64_t currentTimeUs)
{
    TransmissionTimeliner::ReadStatus readStatus = mTransmissionTimeliner.readTask(currentTimeUs);

    // WARNING: The below is handled with care so that the transmitter pointer is guaranteed to be
    //          valid if not set to nullptr. Peripherals are only deleted in 2 places below*

    // See if there is anything to receive
    if (readStatus.busPhase == MapleBusInterface::Phase::READ_COMPLETE)
    {
        // Reset failure count
        mCommFailCount = 0;

        // Check addresses to determine what sub nodes are attached
        uint8_t sendAddr = readStatus.received->frame.senderAddr;
        uint8_t recAddr = readStatus.received->frame.recipientAddr;
        if ((recAddr & 0x3F) == 0x00)
        {
            // This packet was meant for me (the host)

            if (sendAddr & mAddr)
            {
                bool changeDetected = false;
                // This was meant for the main node or one of the main node's peripherals
                // Use the sender address to determine what sub peripherals are connected
                for (std::vector<std::shared_ptr<DreamcastSubNode>>::iterator iter = mSubNodes.begin();
                     iter != mSubNodes.end();
                     ++iter)
                {
                    // * A sub peripheral may be deleted because of the following, but we already
                    //   verified that this message was destined for the main node

                    uint8_t mask = (*iter)->getAddr();
                    if ((*iter)->setConnected((sendAddr & mask) != 0, currentTimeUs))
                    {
                        changeDetected = true;
                    }
                }

                if (changeDetected)
                {
                    peripheralChangeEvent(currentTimeUs);
                }
            }
        }

        // Send this off to the one who transmitted this
        const std::shared_ptr<Transmitter>& spTransmitter = readStatus.transmission->spTransmitter;
        Transmitter* transmitter = readStatus.transmission->transmitter;

        if (!transmitter)
        {
            transmitter = spTransmitter.get();
        }

        if (transmitter)
        {
            transmitter->txComplete(readStatus.received, readStatus.transmission);
        }
    }
    else if (readStatus.busPhase == MapleBusInterface::Phase::WRITE_COMPLETE)
    {
        // Send this off to the one who transmitted this
        const std::shared_ptr<Transmitter>& spTransmitter = readStatus.transmission->spTransmitter;
        Transmitter* transmitter = readStatus.transmission->transmitter;

        if (!transmitter)
        {
            transmitter = spTransmitter.get();
        }

        if (transmitter)
        {
            transmitter->txComplete(readStatus.received, readStatus.transmission);
        }
    }
    else if (readStatus.busPhase == MapleBusInterface::Phase::READ_FAILED
             || readStatus.busPhase == MapleBusInterface::Phase::WRITE_FAILED)
    {
        // Send this off to the one who transmitted this
        const std::shared_ptr<Transmitter>& spTransmitter = readStatus.transmission->spTransmitter;
        Transmitter* transmitter = readStatus.transmission->transmitter;

        if (!transmitter)
        {
            transmitter = spTransmitter.get();
        }

        if (transmitter)
        {
            transmitter->txFailed(readStatus.busPhase == MapleBusInterface::Phase::WRITE_FAILED,
                                    readStatus.busPhase == MapleBusInterface::Phase::READ_FAILED,
                                    readStatus.transmission);
        }

        uint8_t recipientAddr = readStatus.transmission->packet->frame.recipientAddr;
        if ((recipientAddr & mAddr) && ++mCommFailCount >= MAX_FAILURE_DISCONNECT_COUNT)
        {
            // A transmission failure on a main node must cause peripheral disconnect
            if (mPeripherals.size() > 0)
            {
                disconnectMainPeripheral(currentTimeUs);
            }
            mCommFailCount = 0;
        }
    }

    if (mSendConnectedSignal)
    {
        peripheralChangeEvent(currentTimeUs);
        mSendConnectedSignal = false;
    }
    else if (currentTimeUs >= mChangeReleaseTime)
    {
        mPlayerData->gamepad.setChangeCondition(false);
        mChangeReleaseTime = 0;
    }
}

void DreamcastMainNode::runDependentTasks(uint64_t currentTimeUs)
{
    // Have the connected main peripheral and sub nodes handle their tasks
    handlePeripherals(currentTimeUs);

    for (std::vector<std::shared_ptr<DreamcastSubNode>>::iterator iter = mSubNodes.begin();
            iter != mSubNodes.end();
            ++iter)
    {
        (*iter)->task(currentTimeUs);
    }

    if (mPeripherals.size() > 0)
    {
        // The main node peripheral MUST have a recurring transmission in order to test for heartbeat
        if (mEndpointTxScheduler->countRecipients(getRecipientAddress()) == 0)
        {
            uint64_t txTime = PrioritizedTxScheduler::computeNextTimeCadence(currentTimeUs, US_PER_CHECK);
            mEndpointTxScheduler->add(
                EndpointTxSchedulerInterface::TransmissionProperties{
                    .txTime = txTime,
                    .command = COMMAND_DEVICE_INFO_REQUEST,
                    .payload = nullptr,
                    .payloadLen = 0,
                    .expectResponse = true,
                    .expectedResponseNumPayloadWords = EXPECTED_DEVICE_INFO_PAYLOAD_WORDS
                },
                nullptr
            );
        }
    }

    // Summary is printed here for multi-core safety/serialization
    if (mPrintSummary)
    {
        mPrintSummary = false;

        printPeripherals();

        for (std::vector<std::shared_ptr<DreamcastSubNode>>::iterator iter = mSubNodes.begin();
            iter != mSubNodes.end();
            ++iter)
        {
            printf(",");
            (*iter)->printPeripherals();
        }
        printf("\n");
    }

    // Summary is retrieved here for multi-core safety/serialization
    if (mSummaryCallback)
    {
        std::list<std::list<std::array<uint32_t, 2>>> summary;

        bool add = false;
        for (std::vector<std::shared_ptr<DreamcastSubNode>>::reverse_iterator iter = mSubNodes.rbegin();
            iter != mSubNodes.rend();
            ++iter)
        {
            std::list<std::array<uint32_t, 2>> p = (*iter)->getPeripherals();
            if (add || !p.empty())
            {
                summary.push_front(std::move(p));
                add = true;
            }
        }

        summary.push_front(getPeripherals());

        std::function<void(const std::list<std::list<std::array<uint32_t, 2>>>&)> callback;

        mSummaryCallback.swap(callback);

        callback(summary);
    }
}

void DreamcastMainNode::writeTask(uint64_t currentTimeUs)
{
    // Handle transmission
    std::shared_ptr<const Transmission> sentTx =
        mTransmissionTimeliner.writeTask(currentTimeUs);

    if (sentTx != nullptr)
    {
        // Send this off to the one who transmitted this
        const std::shared_ptr<Transmitter>& spTransmitter = sentTx->spTransmitter;
        Transmitter* transmitter = sentTx->transmitter;

        if (!transmitter)
        {
            transmitter = spTransmitter.get();
        }

        if (transmitter)
        {
            transmitter->txStarted(sentTx);
        }
    }
}

void DreamcastMainNode::task(uint64_t currentTimeUs)
{
    readTask(currentTimeUs);
    runDependentTasks(currentTimeUs);
    writeTask(currentTimeUs);
}

void DreamcastMainNode::addInfoRequestToSchedule(uint64_t currentTimeUs)
{
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
            .autoRepeatUs = (mDetectionOnly ? DETECTION_ONLY_US_PER_CHECK : US_PER_CHECK)
        },
        this
    );
}

void DreamcastMainNode::cancelInfoRequest()
{
    if (mScheduleId >= 0)
    {
        mEndpointTxScheduler->cancelById(mScheduleId);
        mScheduleId = -1;
    }
}

void DreamcastMainNode::peripheralChangeEvent(uint64_t currentTimeUs)
{
    mPlayerData->gamepad.setChangeCondition(true);
    mChangeReleaseTime = currentTimeUs + (CONNECT_EVENT_SIGNAL_TIME_MS * 1000);
}
