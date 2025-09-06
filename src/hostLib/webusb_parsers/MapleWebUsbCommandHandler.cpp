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

#include "MapleWebUsbCommandHandler.hpp"

#include "hal/MapleBus/MaplePacket.hpp"
#include "hal/System/LockGuard.hpp"

#include <cstring>
#include <vector>
#include <cinttypes>

MapleWebUsbCommandHandler::MapleWebUsbCommandHandler(const std::map<uint8_t, DreamcastNodeData>& dcNodes) :
    mDcNodes(dcNodes)
{}

std::pair<DreamcastNodeData*, MaplePacket::Frame> MapleWebUsbCommandHandler::processMaplePacket(
    const std::uint8_t* payload,
    std::uint16_t payloadLen,
    const std::function<
        void(std::uint8_t responseCmd, const std::list<std::pair<const void*, std::uint16_t>>& payloadList)
    >& responseFn
)
{
    const std::uint8_t* eol = payload + payloadLen;
    const std::uint8_t* iter = payload;

    if (iter >= eol)
    {
        responseFn(kResponseCmdInvalid, {});
        return std::make_pair(nullptr, MaplePacket::Frame::defaultFrame());
    }

    std::uint16_t size = payloadLen;

    uint16_t wordLen = size / 4;
    if (wordLen < 1 || wordLen > 256 || (size - (wordLen * 4) != 0))
    {
        // Invalid - too few words, too many words, or number of bytes not divisible by 4
        std::uint8_t payload = 0;
        responseFn(kResponseFailure, {{&payload, 1}});
        return std::make_pair(nullptr, MaplePacket::Frame::defaultFrame());
    }

    // Incoming data will be in network order
    const MaplePacket::ByteOrder byteOrder = MaplePacket::ByteOrder::NETWORK;

    // memcpy is used in order to avoid casting from uint8* to uint32* - the RP2040 would get cranky that way
    uint32_t frameWord;
    memcpy(&frameWord, iter, 4);
    MaplePacket::Frame frame = MaplePacket::Frame::fromWord(frameWord, byteOrder);
    iter += 4;
    --wordLen;
    std::vector<std::uint32_t> maplePayload(wordLen);
    memcpy(maplePayload.data(), iter, 4 * wordLen);

    MaplePacket packet(frame, std::move(maplePayload), byteOrder);

    if (!packet.isValid())
    {
        // Built up packet is not valid
        std::uint8_t payload = 1;
        responseFn(kResponseFailure, {{&payload, 1}});
        return std::make_pair(nullptr, MaplePacket::Frame::defaultFrame());
    }

    uint8_t sender = packet.frame.senderAddr;
    DreamcastNodeData* pDcNode = nullptr;

    uint8_t availableNodes = 0;
    for (std::pair<const uint8_t, DreamcastNodeData>& node : mDcNodes)
    {
        if (!node.second.playerDef->autoDetectOnly)
        {
            ++availableNodes;
            pDcNode = &node.second;
        }
    }

    if (availableNodes == 1)
    {
        // Single player special case - always send to the one available, regardless of address
        const uint8_t hostAddr = pDcNode->playerDef->mapleHostAddr;
        packet.frame.senderAddr = hostAddr;
        packet.frame.recipientAddr = (packet.frame.recipientAddr & 0x3F) | hostAddr;
    }
    else
    {
        for (std::pair<const uint8_t, DreamcastNodeData>& node : mDcNodes)
        {
            if (sender == node.second.playerDef->mapleHostAddr)
            {
                pDcNode = &node.second;
            }
        }
    }

    if (!pDcNode || pDcNode->playerDef->autoDetectOnly)
    {
        // Couldn't find the desired address
        std::uint8_t payload = 2;
        responseFn(kResponseFailure, {{&payload, 1}});
        return std::make_pair(nullptr, MaplePacket::Frame::defaultFrame());
    }

    class MaplePassthroughTransmitter : public Transmitter
    {
    public:
        MaplePassthroughTransmitter(
            const std::function<
                void(std::uint8_t responseCmd, const std::list<std::pair<const void*, std::uint16_t>>& payloadList)
            >& responseFn
        ):
            mResponseFn(responseFn)
        {}

        void txStarted(std::shared_ptr<const Transmission> tx) override
        {}

        void txFailed(
            bool writeFailed,
            bool readFailed,
            std::shared_ptr<const Transmission> tx
        ) override
        {
            std::uint8_t payload = writeFailed ? 3 : 4;
            mResponseFn(kResponseFailure, {{&payload, 1}});
        }

        void txComplete(
            std::shared_ptr<const MaplePacket> packet,
            std::shared_ptr<const Transmission> tx
        )
        {
            // Retrieves frame word in packet order
            std::uint32_t frameWord = packet->getFrameWord();
            mResponseFn(
                kResponseSuccess,
                {
                    {&frameWord, sizeof(frameWord)},
                    {packet->payload.data(), sizeof(packet->payload[0]) * packet->payload.size()}
                }
            );
        }

        std::function<
            void(std::uint8_t responseCmd, const std::list<std::pair<const void*, std::uint16_t>>& payloadList)
        > mResponseFn;
    };

    pDcNode->scheduler->add(
        PrioritizedTxScheduler::TransmissionProperties{
            .priority = PrioritizedTxScheduler::EXTERNAL_TRANSMISSION_PRIORITY,
            .txTime = PrioritizedTxScheduler::TX_TIME_ASAP,
            .packet = std::move(packet),
            .expectResponse = true,
            .rxByteOrder = MaplePacket::ByteOrder::NETWORK // Network order!
        },
        std::make_shared<MaplePassthroughTransmitter>(responseFn)
    );

    return std::make_pair(pDcNode, frame);
}

void MapleWebUsbCommandHandler::process(
    const std::uint8_t* payload,
    std::uint16_t payloadLen,
    const std::function<
        void(std::uint8_t responseCmd, const std::list<std::pair<const void*, std::uint16_t>>& payloadList)
    >& responseFn
)
{
    static_cast<void>(
        processMaplePacket(payload, payloadLen, responseFn)
    );
}
