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

#pragma once

#include "hal/System/MutexInterface.hpp"

#include <cstdint>
#include <vector>
#include <memory>
#include <functional>
#include <list>

//! Command parser for processing commands from a WebUSB serial stream
class WebUsbCommandHandler
{
public:
    //! Virtual destructor
    virtual ~WebUsbCommandHandler() = default;

    //! @returns the command byte this parser handles
    virtual std::uint8_t getSupportedCommand() const = 0;

    //! Called when a command has been received and routed to this parser
    //! @param[in] payload The payload received - this is the full payload received by the command without CRC
    //! @param[in] payloadLen The number of bytes in \p payload
    //! @param[in] responseFn The function to call to make a response
    virtual void process(
        const std::uint8_t* payload,
        std::uint16_t payloadLen,
        const std::function<
            void(std::uint8_t responseCmd, const std::list<std::pair<const void*, std::uint16_t>>& payloadList)
        >& responseFn
    ) = 0;

    //! Flip endianness
    //! @tparam T The input/output type
    //! @param[in] val The input value
    //! @return Output value with endian flipped
    template <typename T>
    static constexpr T flipEndian(T val) noexcept
    {
        static_assert(std::is_integral_v<T>, "Type must be integral");

        auto* ptr = reinterpret_cast<std::byte*>(&val);
        std::reverse(ptr, ptr + sizeof(T));
        return val;
    }

    //! Append an integer to a string output response
    //! @tparam T Input integer type
    //! @param[out] response The response to append to
    //! @param[in] val The input value
    template <typename T>
    static void appendIntToResponse(std::string& response, T val)
    {
        T out = flipEndian(val);
        const char* const pOut = reinterpret_cast<const char*>(&out);
        response.append(pOut, sizeof(T));
    }

    static constexpr const std::uint8_t kResponseSuccess = 0x0A;
    static constexpr const std::uint8_t kResponseAttention = 0x0B;
    static constexpr const std::uint8_t kResponseFailure = 0x0F;

    static constexpr const std::uint8_t kResponseCmdInvalid = 0xFE;
};

void webusb_init(MutexInterface* mutex);
void webusb_add_parser(std::shared_ptr<WebUsbCommandHandler> parser);
void webusb_process();
void webusb_flush_outgoing();
