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

#ifndef __CRITICAL_SECTION_MUTEX_H__
#define __CRITICAL_SECTION_MUTEX_H__

#include "hal/System/MutexInterface.hpp"
#include "pico/critical_section.h"

// From raspberry-pi-pico-c-sdk.pdf:
// Critical Section API for short-lived mutual exclusion safe for IRQ and multi-core.
// A critical section is non-reentrant, and provides mutual exclusion using a spin-lock to prevent
// access from the other core, and from (higher priority) interrupts on the same core. It does the
// former using a spin lock and the latter by disabling interrupts on the calling core. Because
// interrupts are disabled when a critical_section is owned, uses of the critical_section should be
// as short as possible.
class CriticalSectionMutex : public MutexInterface
{
    public:
        CriticalSectionMutex();

        virtual ~CriticalSectionMutex();

        virtual void lock() final;

        virtual void unlock() final;

        virtual int8_t tryLock() final;

    private:
        critical_section_t mCriticalSection;
};

#endif // __CRITICAL_SECTION_MUTEX_H__
