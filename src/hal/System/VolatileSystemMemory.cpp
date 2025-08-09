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

#include "VolatileSystemMemory.hpp"
#include <string.h>

#include "pico/stdlib.h"

VolatileSystemMemory::VolatileSystemMemory(uint32_t size) :
    SystemMemory(),
    mSize(size),
    mMemory(new uint8_t[mSize]),
    mLastActivityTime(0)
{
    memset(mMemory.get(), 0xFF, mSize);
}

uint32_t VolatileSystemMemory::getMemorySize()
{
    return mSize;
}

const uint8_t* VolatileSystemMemory::read(uint32_t offset, uint32_t& size)
{
    mLastActivityTime = time_us_64();

    const uint8_t* mem = nullptr;
    if (offset >= mSize)
    {
        size = 0;
    }
    else
    {
        mem = &mMemory[offset];
        uint32_t max = mSize - offset;
        size = (max >= size) ? size : max;
    }
    return mem;
}

bool VolatileSystemMemory::write(uint32_t offset, const void* data, uint32_t& size)
{
    mLastActivityTime = time_us_64();

    bool success = false;
    uint8_t* mem = nullptr;
    if (offset >= mSize)
    {
        size = 0;
    }
    else
    {
        mem = &mMemory[offset];
        uint32_t max = mSize - offset;
        if (max >= size)
        {
            success = true;
        }
        else
        {
            size = max;
        }
        memcpy(mem, data, size);
    }
    return success;
}

uint64_t VolatileSystemMemory::getLastActivityTime()
{
    // WARNING: Not an atomic read, but this isn't a critical thing anyway
    return mLastActivityTime;
}
