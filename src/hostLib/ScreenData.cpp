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

#include "ScreenData.hpp"
#include <cstring>
#include <assert.h>
#include "hal/System/LockGuard.hpp"
#include "utils.h"

const uint32_t ScreenData::DEFAULT_SCREENS[ScreenData::NUM_DEFAULT_SCREENS][ScreenData::NUM_SCREEN_WORDS] = {
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000577, 0xEDFC0000, 0x5B134904, 0x000076D7, 0x61740FC0,
        0x408A4574, 0x1FE02FF5, 0x8D743870, 0x447B7D04, 0x37B00D7F, 0x6DFC37B0, 0x0C491400, 0x30303FCF,
        0xCB6C3FF0, 0x1925083C, 0x1FE04C26, 0x4BCC0000, 0x3432D69C, 0x000008E7, 0x3FD80000, 0x56375488,
        0x186055EF, 0xE5901860, 0x7F36B4E4, 0x1860434E, 0x0F581860, 0x4BE878F0, 0x1FE0160F, 0xF1441FE0,
        0x36B2722C, 0x186074EF, 0x6FE41860, 0x0040E000, 0x18607F55, 0x55FC1860, 0x4101C104, 0x1FE05D3D,
        0xF5740FC0, 0x5D4EC174, 0x00005D32, 0xE5740000, 0x412DB504, 0x00007F12, 0xA5FC0000, 0x00000000
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000577, 0xEDFC0000, 0x5B134904, 0x000076D7, 0x61740FC0,
        0x408A4574, 0x1FE02FF5, 0x8D743870, 0x447B7D04, 0x37B00D7F, 0x6DFC37B0, 0x0C491400, 0x30303FCF,
        0xCB6C3FF0, 0x1925083C, 0x1FE04C26, 0x4BCC0000, 0x3432D69C, 0x000008E7, 0x3FD80000, 0x56375488,
        0x0FE055EF, 0xE5901FE0, 0x7F36B4E4, 0x1860434E, 0x0F581860, 0x4BE878F0, 0x1860160F, 0xF1440FE0,
        0x36B2722C, 0x0FE074EF, 0x6FE41860, 0x0040E000, 0x18607F55, 0x55FC1860, 0x4101C104, 0x1FE05D3D,
        0xF5740FE0, 0x5D4EC174, 0x00005D32, 0xE5740000, 0x412DB504, 0x00007F12, 0xA5FC0000, 0x00000000
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000577, 0xEDFC0000, 0x5B134904, 0x000076D7, 0x61740FC0,
        0x408A4574, 0x1FE02FF5, 0x8D743870, 0x447B7D04, 0x37B00D7F, 0x6DFC37B0, 0x0C491400, 0x30303FCF,
        0xCB6C3FF0, 0x1925083C, 0x1FE04C26, 0x4BCC0000, 0x3432D69C, 0x000008E7, 0x3FD80000, 0x56375488,
        0x0FC055EF, 0xE5901FE0, 0x7F36B4E4, 0x1860434E, 0x0F580060, 0x4BE878F0, 0x0060160F, 0xF1440060,
        0x36B2722C, 0x006074EF, 0x6FE40060, 0x0040E000, 0x00607F55, 0x55FC1860, 0x4101C104, 0x1FE05D3D,
        0xF5740FC0, 0x5D4EC174, 0x00005D32, 0xE5740000, 0x412DB504, 0x00007F12, 0xA5FC0000, 0x00000000
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000577, 0xEDFC0000, 0x5B134904, 0x000076D7, 0x61740FC0,
        0x408A4574, 0x1FE02FF5, 0x8D743870, 0x447B7D04, 0x37B00D7F, 0x6DFC37B0, 0x0C491400, 0x30303FCF,
        0xCB6C3FF0, 0x1925083C, 0x1FE04C26, 0x4BCC0000, 0x3432D69C, 0x000008E7, 0x3FD80000, 0x56375488,
        0x07E055EF, 0xE5900FE0, 0x7F36B4E4, 0x1C60434E, 0x0F581860, 0x4BE878F0, 0x1860160F, 0xF1441860,
        0x36B2722C, 0x186074EF, 0x6FE41860, 0x0040E000, 0x18607F55, 0x55FC1C60, 0x4101C104, 0x0FE05D3D,
        0xF57407E0, 0x5D4EC174, 0x00005D32, 0xE5740000, 0x412DB504, 0x00007F12, 0xA5FC0000, 0x00000000
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
