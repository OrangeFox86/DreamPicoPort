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

#include "DreamcastScreen.hpp"

namespace client
{

DreamcastScreen::DreamcastScreen(ScreenFn callback, uint8_t width, uint8_t height) :
    DreamcastPeripheralFunction(DEVICE_FN_LCD),
    mWidth(width),
    mHeight(height),
    mCallback(callback)
{}

//! Handle packet meant for this peripheral function
//! @param[in] in  The packet read from the Maple Bus
//! @param[out] out  The packet to write to the Maple Bus when true is returned
//! @returns true iff the packet was handled
bool DreamcastScreen::handlePacket(const MaplePacket& in, MaplePacket& out)
{
    const uint8_t cmd = in.frame.command;
    switch (cmd)
    {
        case COMMAND_GET_MEMORY_INFORMATION:
        {
            out.frame.command = COMMAND_RESPONSE_DATA_XFER;
            out.reservePayload(2);
            out.appendPayload(getFunctionCode());
            out.appendPayload(((mWidth - 1) << 24) | ((mHeight - 1) << 16) | 0x1002);
            return true;
        }
        break;

        case COMMAND_BLOCK_WRITE:
        {
            if (in.payload.size() > 2)
            {
                if (mCallback)
                {
                    mCallback(in.payload.data() + 2, in.payload.size() - 2);
                }
                out.frame.command = COMMAND_RESPONSE_ACK;
                return true;
            }
        }
        break;

        case COMMAND_SET_CONDITION:
        {
            out.frame.command = COMMAND_RESPONSE_DATA_XFER;
            out.reservePayload(2);
            out.appendPayload(getFunctionCode());
            out.appendPayload(0);
            return true;
        }
        break;

        default:
        {
        }
        break;
    }

    return false;
}

//! Called when player index changed or timeout occurred
void DreamcastScreen::reset()
{
    if (mCallback)
    {
        uint32_t words = ((mWidth * mHeight) + 31) / 32;
        uint32_t dat[words] = {};
        mCallback(dat, words);
    }
}

//! @returns the function definition for this peripheral function
uint32_t DreamcastScreen::getFunctionDefinition()
{
    uint32_t numBytes = (mWidth * mHeight) / 8;
    uint32_t numBlocks = numBytes / 32;

    return 0x00001000 | (((numBlocks - 1) & 0xFF) << 16);
}

}
