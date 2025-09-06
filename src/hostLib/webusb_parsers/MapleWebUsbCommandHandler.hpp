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

#include "hal/Usb/WebUsbCommandHandler.hpp"
#include "hal/System/SystemIdentification.hpp"
#include "hal/System/MutexInterface.hpp"

#include "PrioritizedTxScheduler.hpp"

#include "PlayerData.hpp"
#include "DreamcastMainNode.hpp"
#include "DreamcastNodeData.hpp"

#include <memory>
#include <list>
#include <array>
#include <functional>

class MapleWebUsbCommandHandler : public WebUsbCommandHandler
{
public:
    MapleWebUsbCommandHandler(const std::map<uint8_t, DreamcastNodeData>& dcNodes);

    virtual ~MapleWebUsbCommandHandler() = default;

    //! Inherited from WebUsbCommandHandler
    inline std::uint8_t getSupportedCommand() const override
    {
        return static_cast<std::uint8_t>('0');
    }

    //! Processes a raw maple packet
    //! @param[in] payload Full maple packet payload, excluding CRC
    //! @param[in] payloadLen The length of \p payload
    //! @param[in] responseFn The function to respond on
    //! @return [nullptr, MaplePacket::Frame::defaultFrame()] on failure
    //! @return pair where the first value is the DC node and the second value is the frame word transmitted to the node
    std::pair<DreamcastNodeData*, MaplePacket::Frame> processMaplePacket(
        const std::uint8_t* payload,
        std::uint16_t payloadLen,
        const std::function<
            void(std::uint8_t responseCmd, const std::list<std::pair<const void*, std::uint16_t>>& payloadList)
        >& responseFn
    );

    //! Inherited from WebUsbCommandHandler
    void process(
        const std::uint8_t* payload,
        std::uint16_t payloadLen,
        const std::function<
            void(std::uint8_t responseCmd, const std::list<std::pair<const void*, std::uint16_t>>& payloadList)
        >& responseFn
    ) override;

private:
    std::map<uint8_t, DreamcastNodeData> mDcNodes;
};
