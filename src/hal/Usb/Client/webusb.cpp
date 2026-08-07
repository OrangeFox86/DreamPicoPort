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

#include "webusb.hpp"
#include "tusb.h"
#include "tusb_config.h"
#include "usb_descriptors.h"
#include "hal/System/LockGuard.hpp"
#include "hal/Usb/WebUsbProcessorBase.hpp"
#include "utils.h"

#include <string>
#include <cstdint>
#include <cstddef>
#include <unordered_map>
#include <array>
#include <vector>
#include <atomic>
#include <algorithm>
#include <memory>
#include <deque>

class WebUsbProcessor : public WebUsbProcessorBase
{
public:
    //! Default constructor (deleted)
    WebUsbProcessor() = delete;

    //! Constructor
    //! @param[in] itf The interface index
    WebUsbProcessor(uint8_t itf) : WebUsbProcessorBase(itf) {}

    //! Add a parser to my static parser dictionary
    //! @param[in] parser The parser to add
    static void addParser(const std::shared_ptr<WebUsbCommandHandler>& parser)
    {
        if (parser)
        {
            uint8_t cmd = parser->getSupportedCommand();
            mParsers[cmd] = parser;
        }
    }

private:
    //! Process a received packet
    //! @param[in] address Address bytes
    //! @param[in] cmd Packet's command byte
    //! @param[in] payload Pointer to the beginning of the payload
    //! @param[in] payloadLen
    void processPkt(const std::string& address, const uint8_t cmd, const uint8_t* payload, uint16_t payloadLen) override
    {
        std::unordered_map<std::uint8_t, std::shared_ptr<WebUsbCommandHandler>>::iterator iter = mParsers.find(cmd);
        if (iter != mParsers.end() && iter->second)
        {
            iter->second->process(
                payload,
                payloadLen,
                [this, address = address]
                (std::uint8_t responseCmd, const std::list<std::pair<const void*, std::uint16_t>>& payloadList) -> void
                {
                    sendPkt(address, responseCmd, payloadList);
                }
            );
        }
        else
        {
            // Unsupported command
            sendPkt(address, kCmdBadCmd, {{&cmd, sizeof(cmd)}});
        }
    }

    //! Perform vendor write to USB
    //! @param[in] buffer Buffer to write
    //! @param[in] bufsize The number of bytes to write
    //! @param[in] flush When true, perform flush
    //! @param[in] task When true, perform usb task before flush (ignored when flush is false)
    //! @return total number of bytes written
    std::uint32_t vendorWrite(
        const void* buffer,
        std::uint32_t bufsize,
        bool flush = false,
        bool task = false
    ) override
    {
        const std::uint8_t* buffer8 = reinterpret_cast<const std::uint8_t*>(buffer);
        std::uint32_t consecutiveFailures = 0;
        std::uint32_t totalWritten = 0;
        while (bufsize > 0)
        {
            std::uint32_t written = tud_vendor_n_write(mItf, buffer8, bufsize);

            if (written == 0 && bufsize > 0)
            {
                ++consecutiveFailures;
                if (consecutiveFailures >= 2)
                {
                    // Failed to write twice - return now
                    return totalWritten;
                }
            }
            else
            {
                consecutiveFailures = 0;
            }

            written = (written >= bufsize) ? bufsize : written;
            bufsize -= written;
            buffer8 += written;

            if (bufsize > 0 || flush)
            {
                if (task)
                {
                    tud_task();
                }
                tud_vendor_n_write_flush(mItf);
            }

            totalWritten += written;
        }

        if (flush)
        {
            // Ensure final flush
            if (task)
            {
                tud_task();
            }
            tud_vendor_n_write_flush(mItf);
        }

        return totalWritten;
    }

private:
    //! All parsers
    static std::unordered_map<std::uint8_t, std::shared_ptr<WebUsbCommandHandler>> mParsers;
};

// definition of mParsers
std::unordered_map<std::uint8_t, std::shared_ptr<WebUsbCommandHandler>> WebUsbProcessor::mParsers;

//! All available interfaces, mapped by interface number [0, CFG_TUD_VENDOR)
static std::vector<WebUsbProcessor> webusb_interfaces = []() {
    std::vector<WebUsbProcessor> arr;
    arr.reserve(CFG_TUD_VENDOR);
    for (uint8_t i = 0; i < CFG_TUD_VENDOR; ++i) {
        arr.push_back(WebUsbProcessor(i));
    }
    return arr;
}();

void webusb_init(MutexInterface* mutex)
{
    for(WebUsbProcessor& itf : webusb_interfaces)
    {
        itf.init(mutex);
    }
}

void webusb_connection_event(uint16_t interfaceNumber, uint16_t value)
{
    // Called from USB core (core 0)
    uint8_t index = ITF_TO_WEBUSB_IDX(interfaceNumber);
    if (index < webusb_interfaces.size() && value < 4)
    {
        // value
        // 0: Disconnect
        // 1: Connect and send zeros to shift out last command
        // 2: Connect and send null command
        // 3: Connect only

        // Connected or disconnected. In either case, clear write buffer.
        tud_vendor_n_write_clear(index);

        if (value != 0)
        {
            webusb_interfaces[index].connect(value == 1, value == 2);
        }
        else
        {
            webusb_interfaces[index].disconnect();
        }
    }
}

void webusb_rx(uint8_t itfIndex, const uint8_t* buffer, uint16_t bufsize)
{
    // Called from USB core (core 0)
    if (itfIndex < webusb_interfaces.size())
    {
        webusb_interfaces[itfIndex].addData(buffer, bufsize);
    }
}

void webusb_process()
{
    // Called from Maple core (core 1)
    for (WebUsbProcessor& itf : webusb_interfaces)
    {
        itf.process();
    }
}

// Drain outgoing queue and perform TinyUSB writes on core0 only.
void webusb_flush_outgoing()
{
    for(WebUsbProcessor& itf : webusb_interfaces)
    {
        itf.flushOutgoing();
    }
}

void webusb_add_parser(std::shared_ptr<WebUsbCommandHandler> parser)
{
    WebUsbProcessor::addParser(parser);
}
