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

#pragma once

#include "hal/Usb/WebUsbCommandParser.hpp"
#include "hal/System/SystemIdentification.hpp"
#include "hal/System/MutexInterface.hpp"

#include "PrioritizedTxScheduler.hpp"

#include "PlayerData.hpp"
#include "DreamcastMainNode.hpp"

#include <memory>
#include <list>
#include <array>
#include <functional>

class MapleWebUsbParser : public WebUsbCommandParser
{
public:
    MapleWebUsbParser(
        const std::vector<std::shared_ptr<PrioritizedTxScheduler>>& schedulers,
        const std::vector<uint8_t>& senderAddresses
    );

    virtual ~MapleWebUsbParser() = default;

    //! Inherited from WebUsbCommandParser
    inline std::uint8_t getSupportedCommand() const override
    {
        return static_cast<std::uint8_t>('0');
    }

    //! Processes a raw maple packet
    //! @param[in] payload Full maple packet payload, excluding CRC
    //! @param[in] payloadLen The length of \p payload
    //! @param[in] responseFn The function to respond on
    //! @return [-1, MaplePacket::Frame::defaultFrame()] on failure
    //! @return pair where the first value is the player index and the second value is the frame word transmitted
    std::pair<int32_t, MaplePacket::Frame> processMaplePacket(
        const std::uint8_t* payload,
        std::uint16_t payloadLen,
        const std::function<
            void(std::uint8_t responseCmd, const std::list<std::pair<const void*, std::uint16_t>>& payloadList)
        >& responseFn
    );

    //! Inherited from WebUsbCommandParser
    void process(
        const std::uint8_t* payload,
        std::uint16_t payloadLen,
        const std::function<
            void(std::uint8_t responseCmd, const std::list<std::pair<const void*, std::uint16_t>>& payloadList)
        >& responseFn
    ) override;

private:
    const std::vector<std::shared_ptr<PrioritizedTxScheduler>> mSchedulers;
    const std::vector<uint8_t> mSenderAddresses;
};
