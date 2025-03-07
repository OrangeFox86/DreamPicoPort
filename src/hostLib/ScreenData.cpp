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

#include "ScreenData.hpp"
#include <cstring>
#include <assert.h>
#include "hal/System/LockGuard.hpp"
#include "utils.h"

const uint32_t ScreenData::DEFAULT_SCREENS[ScreenData::NUM_DEFAULT_SCREENS][ScreenData::NUM_SCREEN_WORDS] = {
    {
        0x1FF8FFFF, 0xFFF80000, 0x00000000, 0x00004E81, 0x5BF80000, 0x46C53208, 0x0000B801, 0xF2E803E0,
        0xBFCE8AE8, 0x0FF04FA6, 0x92E81C38, 0x78895A08, 0x1BD84A87, 0xA3F81BD8, 0xE8FD9800, 0x18182FCA,
        0x16481FF8, 0xCFDDF080, 0x0FF0CD31, 0xF7A80000, 0x97242DA0, 0x0000419F, 0x16980000, 0x5BFB5C00,
        0x0C30F68C, 0x02780C30, 0x004424B8, 0x0C30D7D8, 0xAE580C30, 0x6806AD90, 0x0FF07D68, 0xF6680FF0,
        0x92B60D68, 0x0C300584, 0x4B480C30, 0x0032A000, 0x0C30FEAA, 0xABF80C30, 0x82F3AA08, 0x0FF0BA3B,
        0x92E807E0, 0xBA2732E8, 0x0000BA19, 0xF2E80000, 0x8287C208, 0x0000FE69, 0x83F80000, 0x00000000
    },
    {
        0x1FF8FFFF, 0xFFF80000, 0x00000000, 0x00004E81, 0x5BF80000, 0x46C53208, 0x0000B801, 0xF2E803E0,
        0xBFCE8AE8, 0x0FF04FA6, 0x92E81C38, 0x78895A08, 0x1BD84A87, 0xA3F81BD8, 0xE8FD9800, 0x18182FCA,
        0x16481FF8, 0xCFDDF080, 0x0FF0CD31, 0xF7A80000, 0x97242DA0, 0x0000419F, 0x16980000, 0x5BFB5C00,
        0x07F0F68C, 0x02780FF0, 0x004424B8, 0x0C30D7D8, 0xAE580C30, 0x6806AD90, 0x0C307D68, 0xF66807F0,
        0x92B60D68, 0x07F00584, 0x4B480C30, 0x0032A000, 0x0C30FEAA, 0xABF80C30, 0x82F3AA08, 0x0FF0BA3B,
        0x92E807F0, 0xBA2732E8, 0x0000BA19, 0xF2E80000, 0x8287C208, 0x0000FE69, 0x83F80000, 0x00000000
    },
    {
        0x1FF8FFFF, 0xFFF80000, 0x00000000, 0x00004E81, 0x5BF80000, 0x46C53208, 0x0000B801, 0xF2E803E0,
        0xBFCE8AE8, 0x0FF04FA6, 0x92E81C38, 0x78895A08, 0x1BD84A87, 0xA3F81BD8, 0xE8FD9800, 0x18182FCA,
        0x16481FF8, 0xCFDDF080, 0x0FF0CD31, 0xF7A80000, 0x97242DA0, 0x0000419F, 0x16980000, 0x5BFB5C00,
        0x07E0F68C, 0x02780FF0, 0x004424B8, 0x0C30D7D8, 0xAE580030, 0x6806AD90, 0x00307D68, 0xF6680030,
        0x92B60D68, 0x00300584, 0x4B480030, 0x0032A000, 0x0030FEAA, 0xABF80C30, 0x82F3AA08, 0x0FF0BA3B,
        0x92E807E0, 0xBA2732E8, 0x0000BA19, 0xF2E80000, 0x8287C208, 0x0000FE69, 0x83F80000, 0x00000000
    },
    {
        0x1FF8FFFF, 0xFFF80000, 0x00000000, 0x00004E81, 0x5BF80000, 0x46C53208, 0x0000B801, 0xF2E803E0,
        0xBFCE8AE8, 0x0FF04FA6, 0x92E81C38, 0x78895A08, 0x1BD84A87, 0xA3F81BD8, 0xE8FD9800, 0x18182FCA,
        0x16481FF8, 0xCFDDF080, 0x0FF0CD31, 0xF7A80000, 0x97242DA0, 0x0000419F, 0x16980000, 0x5BFB5C00,
        0x03F0F68C, 0x027807F0, 0x004424B8, 0x0E30D7D8, 0xAE580C30, 0x6806AD90, 0x0C307D68, 0xF6680C30,
        0x92B60D68, 0x0C300584, 0x4B480C30, 0x0032A000, 0x0C30FEAA, 0xABF80E30, 0x82F3AA08, 0x07F0BA3B,
        0x92E803F0, 0xBA2732E8, 0x0000BA19, 0xF2E80000, 0x8287C208, 0x0000FE69, 0x83F80000, 0x00000000
    }
};

ScreenData::ScreenData(MutexInterface& mutex, uint32_t defaultScreenNum) :
    mMutex(mutex),
    mNewDataAvailable(false)
{
    if (defaultScreenNum > NUM_DEFAULT_SCREENS)
    {
        defaultScreenNum = 0;
    }
    std::memcpy(mDefaultScreen, DEFAULT_SCREENS[defaultScreenNum], sizeof(mDefaultScreen));
    resetToDefault();
}

void ScreenData::setData(const uint32_t* data, uint32_t startIndex, uint32_t numWords, bool update)
{
    assert(startIndex + numWords <= sizeof(mScreenData));
    LockGuard lockGuard(mMutex);
    if (lockGuard.isLocked())
    {
        std::memcpy(mScreenData + startIndex, data, numWords * sizeof(uint32_t));
        if (update)
        {
            mNewDataAvailable = true;
        }
    }
    else
    {
        DEBUG_PRINT("FAULT: failed to set screen data\n");
    }
}

void ScreenData::setDataToADefault(uint32_t defaultScreenNum)
{
    if (defaultScreenNum > NUM_DEFAULT_SCREENS)
    {
        defaultScreenNum = 0;
    }
    setData(DEFAULT_SCREENS[defaultScreenNum]);
}

void ScreenData::resetToDefault()
{
    std::memcpy(mScreenData, mDefaultScreen, sizeof(mScreenData));
    // Always force an update
    mNewDataAvailable = true;
}

bool ScreenData::isNewDataAvailable() const
{
    return mNewDataAvailable;
}

void ScreenData::readData(uint32_t* out)
{
    LockGuard lockGuard(mMutex);
    if (lockGuard.isLocked())
    {
        mNewDataAvailable = false;
    }
    else
    {
        DEBUG_PRINT("FAULT: failed to properly read screen data\n");
    }
    // Allow this to happen, even if locking failed
    std::memcpy(out, mScreenData, sizeof(mScreenData));
}
