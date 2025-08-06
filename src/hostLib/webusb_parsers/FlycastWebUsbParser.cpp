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

#include "FlycastWebUsbParser.hpp"

#include "hal/MapleBus/MaplePacket.hpp"
#include "hal/System/LockGuard.hpp"

#include <cstring>

const std::uint8_t FlycastWebUsbParser::kInterfaceVersion[2] = {1, 0};

FlycastWebUsbParser::FlycastWebUsbParser(
    SystemIdentification& identification,
    std::shared_ptr<PrioritizedTxScheduler>* schedulers,
    const uint8_t* senderAddresses,
    uint32_t numSenders,
    const std::vector<std::shared_ptr<PlayerData>>& playerData,
    const std::vector<std::shared_ptr<DreamcastMainNode>>& nodes
) :
    mIdentification(identification),
    mSchedulers(schedulers),
    mSenderAddresses(senderAddresses),
    mNumSenders(numSenders),
    mPlayerData(playerData),
    nodes(nodes)
{}

void FlycastWebUsbParser::process(
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
        return;
    }

    switch(*iter)
    {
        // X- to reset all or one of {X-0, X-1, X-2, X-3} to reset a specific player
        case '-':
        {
            // Remove minus
            ++iter;
            int idx = -1;
            if (iter < eol)
            {
                idx = *iter;
            }

            // Reset screen data
            if (idx < 0)
            {
                // all
                std::uint8_t count = 0;
                for (std::shared_ptr<PlayerData>& playerData : mPlayerData)
                {
                    ++count;
                    playerData->screenData.resetToDefault();
                }
                std::string s = std::to_string(count);
                responseFn(kResponseSuccess, {{&count, 1}});
            }
            else if (static_cast<std::size_t>(idx) < mPlayerData.size())
            {
                mPlayerData[idx]->screenData.resetToDefault();
                std::uint8_t count = 1;
                responseFn(kResponseSuccess, {{&count, 1}});
            }
            else
            {
                responseFn(kResponseFailure, {});
            }
        }
        return;

        // XP[0-4][0-4] to change displayed port character
        case 'P':
        {
            // Remove P
            ++iter;
            int idxin = -1;
            if (iter < eol)
            {
                idxin = *iter++;
            }
            int idxout = -1;
            if (iter < eol)
            {
                idxout = *iter++;
            }

            if (
                idxin >= 0 &&
                static_cast<std::size_t>(idxin) < mPlayerData.size() &&
                idxout >= 0 &&
                static_cast<std::size_t>(idxout) < ScreenData::NUM_DEFAULT_SCREENS
            )
            {
                mPlayerData[idxin]->screenData.setDataToADefault(idxout);
                responseFn(kResponseSuccess, {});
            }
            else
            {
                responseFn(kResponseFailure, {});
            }
        }
        return;

        // XS to return serial
        case 'S':
        {
            char buffer[mIdentification.getSerialSize() + 1] = {0};
            mIdentification.getSerial(buffer, sizeof(buffer) - 1);
            buffer[sizeof(buffer) - 1] = '\0';
            responseFn(kResponseSuccess, {{buffer, strlen(buffer) + 1}});
        }
        return;

        // X?0, X?1, X?2, or X?3 will print summary for the given node index
        case '?':
        {
            // Remove question mark
            ++iter;
            int idx = -1;
            if (iter < eol)
            {
                idx = *iter;
            }

            if (idx >= 0 && static_cast<std::size_t>(idx) < nodes.size())
            {
                nodes[idx]->requestSummary(
                    [responseFn](const std::list<std::list<std::array<uint32_t, 2>>>& summary)
                    {
                        std::string summaryString;
                        summaryString.reserve(512);

                        bool firstI = true;
                        for (const auto& i : summary)
                        {
                            if (!firstI)
                            {
                                summaryString += ';';
                            }

                            bool firstJ = true;
                            for (const auto& j : i)
                            {
                                if (!firstJ)
                                {
                                    summaryString += ',';
                                }

                                summaryString += '{';

                                char buffer[10];
                                snprintf(buffer, sizeof(buffer), "%08lX,", j[0]);
                                summaryString += buffer;
                                snprintf(buffer, sizeof(buffer), "%08lX", j[1]);
                                summaryString += buffer;

                                summaryString += '}';

                                firstJ = false;
                            }

                            firstI = false;
                        }

                        responseFn(kResponseSuccess, {{summaryString.data(), summaryString.size()}});
                    }
                );
            }
            else
            {
                responseFn(kResponseFailure, {});
            }
        }
        return;

        // XV to return interface version
        case 'V':
        {
            responseFn(kResponseSuccess, {{kInterfaceVersion, sizeof(kInterfaceVersion)}});
        }
        return;

        // Maple Bus passthrough
        case 0x05:
        {
            // Remove start character
            ++iter;
            std::uint16_t size = payloadLen - 1;

            bool valid = false;
            MaplePacket::Frame frameWord = MaplePacket::Frame::defaultFrame();
            std::vector<uint32_t> payloadWords;

            if (size >= 4)
            {
                frameWord.command = *iter++;
                frameWord.recipientAddr = *iter++;
                frameWord.senderAddr = *iter++;
                frameWord.length = *iter++;
                size -= 4;

                payloadWords.reserve(size / 4);

                while (((iter + 4) <= eol) && (size >= 4))
                {
                    uint32_t word =
                        (static_cast<uint32_t>(*iter) << 24) |
                        (static_cast<uint32_t>(*(iter + 1)) << 16) |
                        (static_cast<uint32_t>(*(iter + 2)) << 8) |
                        static_cast<uint32_t>(*(iter + 3));

                    payloadWords.push_back(word);

                    iter += 4;
                    size -= 4;
                }

                valid = (size == 0);
            }
            else
            {
                valid = false;
            }

            if (valid)
            {
                MaplePacket packet(frameWord, std::move(payloadWords));
                if (packet.isValid())
                {
                    uint8_t sender = packet.frame.senderAddr;
                    int32_t idx = -1;
                    const uint8_t* senderAddress = mSenderAddresses;

                    if (mNumSenders == 1)
                    {
                        // Single player special case - always send to the one available, regardless of address
                        idx = 0;
                        packet.frame.senderAddr = *senderAddress;
                        packet.frame.recipientAddr = (packet.frame.recipientAddr & 0x3F) | *senderAddress;
                    }
                    else
                    {
                        for (uint32_t i = 0; i < mNumSenders && idx < 0; ++i, ++senderAddress)
                        {
                            if (sender == *senderAddress)
                            {
                                idx = i;
                            }
                        }
                    }

                    if (idx >= 0)
                    {
                        if (packet.frame.command == 0x0C &&
                            packet.frame.length == 0x32 &&
                            packet.payload[0] == DEVICE_FN_LCD &&
                            packet.payload[1] == 0)
                        {
                            // Save screen data
                            mPlayerData[idx]->screenData.setData(&packet.payload[2], 0, 0x30, false);
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
                                std::uint32_t frameWord = packet->frame.toWord();
                                mResponseFn(
                                    kResponseSuccess,
                                    {
                                        {&frameWord, sizeof(frameWord)},
                                        {packet->payload.data(), sizeof(packet->payload[0]) * packet->payload.size()}
                                    }
                                );
                            }

                            const std::function<
                                void(std::uint8_t responseCmd, const std::list<std::pair<const void*, std::uint16_t>>& payloadList)
                            >& mResponseFn;
                        };

                        mSchedulers[idx]->add(
                            PrioritizedTxScheduler::EXTERNAL_TRANSMISSION_PRIORITY,
                            PrioritizedTxScheduler::TX_TIME_ASAP,
                            std::make_shared<MaplePassthroughTransmitter>(responseFn),
                            packet,
                            true
                        );
                    }
                    else
                    {
                        std::uint8_t payload = 2;
                        responseFn(kResponseFailure, {{&payload, 1}});
                    }
                }
                else
                {
                    std::uint8_t payload = 1;
                    responseFn(kResponseFailure, {{&payload, 1}});
                }
            }
            else
            {
                std::uint8_t payload = 0;
                responseFn(kResponseFailure, {{&payload, 1}});
            }
        }
        return;

        default:
            responseFn(kResponseCmdInvalid, {{nullptr, 0}});
            return;
    }
}
