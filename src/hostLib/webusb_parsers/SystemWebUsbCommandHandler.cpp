// MIT License
//
// Copyright (c) 2022-2026 The DreamPicoPort Contributors
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

#include "SystemWebUsbCommandHandler.hpp"

#include <cstring>

SystemWebUsbCommandHandler::SystemWebUsbCommandHandler(
    SystemIdentification& identification,
    ClockInterface& clock,
    const std::map<uint8_t, DreamcastNodeData>& dcNodes
) :
    mIdentification(identification),
    mClock(clock),
    mDcNodes(dcNodes)
{}

void SystemWebUsbCommandHandler::process(
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

    const char cmd = *iter++;
    const std::size_t numPayload = (eol - iter);
    switch(cmd)
    {
        // echo
        case 'e':
        {
            responseFn(kResponseSuccess, {{iter, numPayload}});
        }
        break;

        // serial
        case 'S':
        {
            char buffer[mIdentification.getSerialSize() + 1] = {0};
            mIdentification.getSerial(buffer, sizeof(buffer) - 1);
            buffer[sizeof(buffer) - 1] = '\0';
            responseFn(kResponseSuccess, {{buffer, strlen(buffer) + 1}});
        }
        return;

        // -$0, -$1, -$2, or -$3 will return MapleBus status
        case '$':
        {
            // Remove $
            int idx = -1;
            if (numPayload > 0)
            {
                idx = *iter;
            }

            std::map<uint8_t, DreamcastNodeData>::iterator dcNodeIter = mDcNodes.end();
            if (idx >= 0 && (dcNodeIter = mDcNodes.find(idx)) != mDcNodes.end())
            {
                std::string response;
                response.reserve(108);

                const std::uint64_t now = mClock.getTimeUs();
                appendIntToResponse(response, now);

                DreamcastMainNode::MapleStatus status = dcNodeIter->second.mainNode->getMapleStatus();

                appendIntToResponse(response, static_cast<std::uint32_t>(status.phase));

                appendIntToResponse(response, status.mapleStats.numReads);
                appendIntToResponse(response, status.mapleStats.numNullReads);
                appendIntToResponse(response, status.mapleStats.numReadFailCrc);
                appendIntToResponse(response, status.mapleStats.numReadFailIncomplete);
                appendIntToResponse(response, status.mapleStats.numReadFailOverflow);
                appendIntToResponse(response, status.mapleStats.numReadFailTimeout);
                appendIntToResponse(response, status.mapleStats.lastReadStartTime);
                appendIntToResponse(response, status.mapleStats.lastReadCompleteTime);
                appendIntToResponse(response, status.mapleStats.numWrites);
                appendIntToResponse(response, status.mapleStats.numWriteFail);
                appendIntToResponse(response, status.mapleStats.lastWriteStartTime);
                appendIntToResponse(response, status.mapleStats.lastWriteCompleteTime);

                responseFn(kResponseSuccess, {{response.data(), response.size()}});
            }
            else
            {
                responseFn(kResponseFailure, {});
            }
        }
        return;

        default:
            responseFn(kResponseCmdInvalid, {{nullptr, 0}});
            return;
    }
}
