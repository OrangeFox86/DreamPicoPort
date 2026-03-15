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

#include "I2cDevice.hpp"

#include "hardware/i2c.h"

#include <list>
#include <assert.h>

class I2cDriver
{
public:
    enum class State
    {
        NONE = 0,
        READING,
        WRITING
    };

    I2cDriver(uint32_t i2cIdx, uint32_t baud, uint32_t gpioStart);

    void addDevice(I2cDevice* device);

    //! Does an iterative process of I2C activity
    //! @returns true iff some activity was performed after this call
    bool process();

    //! Processes until idle
    //! @param[in] timeoutMs  Maximum number of milliseconds to wait or -1 to wait until complete;
    //!                       flush(0) is the same as calling process()
    void flush(int32_t timeoutMs=-1);

    bool checkForAbort();

    void start(std::list<I2cDevice*>::iterator iter);

    void end();

public:
    static i2c_inst_t* const I2C_INSTANCES[2];

private:
    i2c_inst_t* const mI2c;
    std::list<I2cDevice*> mDeviceList;
    uint8_t mProcAddr;
    I2cDevice* mProcDevice;
    State mProcState;
    bool mWriteComplete;
    uint32_t mNumRead;
    uint32_t mNumReadWaiting;
    bool mFirstByte;
};
