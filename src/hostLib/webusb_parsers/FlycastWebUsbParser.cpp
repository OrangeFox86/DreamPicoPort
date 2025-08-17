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
#include <cinttypes>

const std::uint8_t FlycastWebUsbParser::kInterfaceVersion[2] = {1, 0};

FlycastWebUsbParser::FlycastWebUsbParser(
    SystemIdentification& identification,
    const std::shared_ptr<MapleWebUsbParser>& mapleWebUsbParser,
    const std::vector<std::shared_ptr<PlayerData>>& playerData,
    const std::vector<std::shared_ptr<DreamcastMainNode>>& nodes
) :
    mIdentification(identification),
    mMapleWebUsbParser(mapleWebUsbParser),
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
                    playerData->screenData->resetToDefault();
                }
                std::string s = std::to_string(count);
                responseFn(kResponseSuccess, {{&count, 1}});
            }
            else if (static_cast<std::size_t>(idx) < mPlayerData.size())
            {
                mPlayerData[idx]->screenData->resetToDefault();
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
                mPlayerData[idxin]->screenData->setDataToADefault(idxout);
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
                        std::string response;
                        response.reserve(64);

                        bool first = true;
                        for (const auto& i : summary)
                        {
                            if (!first)
                            {
                                // Semicolon means start of next peripheral data
                                response.push_back(';');
                            }

                            for (const auto& j : i)
                            {
                                // Pipe means a 4-byte value will follow (first is function code)
                                response.push_back('|');
                                uint32_t out = MaplePacket::flipWordBytes(j[0]);
                                const char* const p8Out = reinterpret_cast<const char*>(&out);
                                response.append(p8Out, 4);

                                // Pipe means a 4-byte value will follow (second is function definitions word)
                                response.push_back('|');
                                out = MaplePacket::flipWordBytes(j[1]);
                                response.append(p8Out, 4);
                            }

                            first = false;
                        }

                        responseFn(kResponseSuccess, {{response.data(), response.size()}});
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

            // Process through the MapleWebUsbParser
            auto rv = mMapleWebUsbParser->processMaplePacket(iter, size, responseFn);

            if (rv.first < 0 || !rv.second.isValid())
            {
                // Failed
                return;
            }

            // Check for special commands to process
            if (rv.second.command == 0x0C && rv.second.length == 0x32 && size >= 204)
            {
                uint32_t fn = 0;
                uint32_t loc = 0;
                memcpy(&fn, iter + 4, 4);
                memcpy(&loc, iter + 8, 4);
                fn = MaplePacket::flipWordBytes(fn);
                loc = MaplePacket::flipWordBytes(loc);
                if (fn == DEVICE_FN_LCD && loc == 0)
                {
                    // Save screen data
                    uint32_t screenWords[48];
                    memcpy(screenWords, iter + 12, 48 * sizeof(uint32_t));
                    for (uint32_t i = 0; i < 48; ++i)
                    {
                        screenWords[i] = MaplePacket::flipWordBytes(screenWords[i]);
                    }
                    mPlayerData[rv.first]->screenData->setData(screenWords, 0, 0x30, false);
                }
            }
        }
        return;

        default:
            responseFn(kResponseCmdInvalid, {{nullptr, 0}});
            return;
    }
}
