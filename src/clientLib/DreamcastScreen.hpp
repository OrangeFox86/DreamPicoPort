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

#include "DreamcastPeripheralFunction.hpp"
#include "dreamcast_constants.h"

#include <string.h>

namespace client
{
class DreamcastScreen : public DreamcastPeripheralFunction
{
public:
    typedef void (*ScreenFn)(const uint32_t* screen, uint32_t len);

public:
    DreamcastScreen(ScreenFn callback, uint8_t width = 48, uint8_t height = 32);

    //! Handle packet meant for this peripheral function
    //! @param[in] in  The packet read from the Maple Bus
    //! @param[out] out  The packet to write to the Maple Bus when true is returned
    //! @returns true iff the packet was handled
    virtual bool handlePacket(const MaplePacket& in, MaplePacket& out) final;

    //! Called when player index changed or timeout occurred
    virtual void reset() final;

    //! @returns the function definition for this peripheral function
    virtual uint32_t getFunctionDefinition() final;

private:
    //! This screen's pixel width
    const uint8_t mWidth;
    //! This screen's pixel height
    const uint8_t mHeight;
    //! Callback function which sets screen data
    ScreenFn mCallback;

};
}
