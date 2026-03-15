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
    SystemIdentification& identification
) :
    mIdentification(identification)
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

        default:
            responseFn(kResponseCmdInvalid, {{nullptr, 0}});
            return;
    }
}
