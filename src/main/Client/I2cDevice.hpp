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

#include <stdint.h>

class I2cDevice
{
public:
    //! Virtual destrictor
    virtual ~I2cDevice() {}

    //! @returns the address of this I2C device
    virtual uint8_t getAddress() = 0;

    //! @param[out] byte  Set to the next byte to send
    //! @param[out] sendStop  Send stop after this byte, send start on next
    //! @returns false if this is the last byte in this sequence
    //! @returns true if there is another byte queued for this sequence
    virtual bool nextByteOut(uint8_t& byte, bool& sendStop) = 0;

    //! @returns true when data is ready for write
    virtual bool isOutAvailable() = 0;

    //! Called to notify the device that write failed for the current sequence.
    //! The device is expected to purge its output buffer and optionally start over.
    //! @param[in] abortReason  The reason write failed (see I2C_IC_TX_ABRT_SOURCE definitions)
    virtual void writeFailed(uint32_t abortReason) = 0;

    //! @param[in] byte  The byte that was read off of the bus
    //! @returns false if this is the last byte to be read
    //! @returns true if another byte is expected
    virtual void nextByteIn(uint8_t byte) = 0;

    //! @returns number of bytes to read when input is expected or 0
    virtual uint32_t isInExpected() = 0;

    //! Called to notify the device that read failed for the current sequence.
    //! The device is expected to purge its input buffer and optionally start over.
    //! @param[in] abortReason  The reason read failed (see I2C_IC_TX_ABRT_SOURCE definitions)
    virtual void readFailed(uint32_t abortReason) = 0;
};
