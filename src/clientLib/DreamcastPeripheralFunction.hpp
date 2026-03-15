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

#include <memory>
#include <vector>
#include <bitset>
#include <assert.h>

#include "hal/MapleBus/MaplePacket.hpp"

namespace client
{

class DreamcastPeripheralFunction
{
public:
    //! Constructor
    //! @param[in] functionCode  The function code (mask) for this peripheral function
    inline DreamcastPeripheralFunction(uint32_t functionCode) :
        mFunctionCode(functionCode)
    {
        // One and only one bit may be set for the function code
        std::bitset<32> leftBits(mFunctionCode);
        assert(leftBits.count() == 1);
    }

    //! Virtual destructor
    virtual inline ~DreamcastPeripheralFunction() {}

    //! Handle packet meant for this peripheral function
    //! @param[in] in  The packet read from the Maple Bus
    //! @param[out] out  The packet to write to the Maple Bus when true is returned
    //! @returns true iff the packet was handled
    virtual bool handlePacket(const MaplePacket& in, MaplePacket& out) = 0;

    //! Called when player index changed or timeout occurred
    virtual void reset() = 0;

    //! @returns function code (mask) for this peripheral function
    uint32_t getFunctionCode() { return mFunctionCode; }

    //! @returns the function definition for this peripheral function
    virtual uint32_t getFunctionDefinition() = 0;

private:
    DreamcastPeripheralFunction() = delete;

protected:
    //! The function code (mask) for this peripheral function
    const uint32_t mFunctionCode;
};

}
