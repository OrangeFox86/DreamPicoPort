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

DreamcastMainNode::DreamcastMainNode(MapleBusInterface& bus,
                                     PlayerData playerData,
                                     std::shared_ptr<PrioritizedTxScheduler> prioritizedTxScheduler) :
    DreamcastNode(DreamcastPeripheral::MAIN_PERIPHERAL_ADDR_MASK,
                  std::make_shared<EndpointTxScheduler>(
                    prioritizedTxScheduler,
                    PrioritizedTxScheduler::MAIN_TRANSMISSION_PRIORITY,
                    DreamcastPeripheral::getRecipientAddress(
                        playerData.playerIndex, DreamcastPeripheral::MAIN_PERIPHERAL_ADDR_MASK)
                  ),
                  playerData),
    mSubNodes(),
    mTransmissionTimeliner(bus, prioritizedTxScheduler),
    mScheduleId(-1),
    mCommFailCount(0),
    mPrintSummary(false)
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
                DreamcastPeripheral::getRecipientAddress(playerData.playerIndex, addr)),
            mPlayerData));
    }
}

DreamcastMainNode::~DreamcastMainNode()
{}

void DreamcastMainNode::txComplete(std::shared_ptr<const MaplePacket> packet,
                                   std::shared_ptr<const Transmission> tx)
{
    // Handle device info from main peripheral
    if (packet != nullptr && packet->frame.command == COMMAND_RESPONSE_DEVICE_INFO)
    {
        if (packet->payload.size() > 3)
        {
            uint32_t mask = peripheralFactory(packet->payload);
            if (mPeripherals.size() > 0)
            {
                // Remove the auto reload device info request transmission from schedule
                if (mScheduleId >= 0)
                {
                    mEndpointTxScheduler->cancelById(mScheduleId);
                    mScheduleId = -1;
                }
                DEBUG_PRINT("P%lu connected (", mPlayerData.playerIndex + 1);
                debugPrintPeripherals();
                DEBUG_PRINT(")\n");

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
                // This was meant for the main node or one of the main node's peripherals
                // Use the sender address to determine what sub peripherals are connected
                for (std::vector<std::shared_ptr<DreamcastSubNode>>::iterator iter = mSubNodes.begin();
                     iter != mSubNodes.end();
                     ++iter)
                {
                    // * A sub peripheral may be deleted because of the following, but we already
                    //   verified that this message was destined for the main node

                    uint8_t mask = (*iter)->getAddr();
                    (*iter)->setConnected((sendAddr & mask) != 0, currentTimeUs);
                }
            }
        }

        // Send this off to the one who transmitted this
        Transmitter* transmitter = readStatus.transmission->transmitter;
        if (transmitter != nullptr)
        {
            transmitter->txComplete(readStatus.received, readStatus.transmission);
        }
    }
    else if (readStatus.busPhase == MapleBusInterface::Phase::WRITE_COMPLETE)
    {
        // Send this off to the one who transmitted this
        Transmitter* transmitter = readStatus.transmission->transmitter;
        if (transmitter != nullptr)
        {
            transmitter->txComplete(readStatus.received, readStatus.transmission);
        }
    }
    else if (readStatus.busPhase == MapleBusInterface::Phase::READ_FAILED
             || readStatus.busPhase == MapleBusInterface::Phase::WRITE_FAILED)
    {
        // Send this off to the one who transmitted this
        Transmitter* transmitter = readStatus.transmission->transmitter;
        if (transmitter != nullptr)
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
                // * This will delete all peripherals for this player - this is why the callback for
                //   the transmitter is NOT subsequently called

                disconnectMainPeripheral(currentTimeUs);
            }
            mCommFailCount = 0;
        }
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
                txTime,
                nullptr,
                COMMAND_DEVICE_INFO_REQUEST,
                nullptr,
                0,
                true,
                EXPECTED_DEVICE_INFO_PAYLOAD_WORDS);
        }
    }

    // Summary is printed here for safety
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
}

void DreamcastMainNode::writeTask(uint64_t currentTimeUs)
{
    // Handle transmission
    std::shared_ptr<const Transmission> sentTx =
        mTransmissionTimeliner.writeTask(currentTimeUs);

    if (sentTx != nullptr)
    {
        // Send this off to the one who transmitted this
        Transmitter* transmitter = sentTx->transmitter;
        if (transmitter != nullptr)
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
        txTime,
        this,
        COMMAND_DEVICE_INFO_REQUEST,
        nullptr,
        0,
        true,
        EXPECTED_DEVICE_INFO_PAYLOAD_WORDS,
        US_PER_CHECK);
}
