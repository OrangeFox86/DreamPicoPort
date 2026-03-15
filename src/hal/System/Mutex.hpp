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

#ifndef __MUTEX_H__
#define __MUTEX_H__

#include "hal/System/MutexInterface.hpp"
#include "pico/mutex.h"

// From raspberry-pi-pico-c-sdk.pdf:
// Mutex API for non IRQ mutual exclusion between cores.
// Unlike critical sections, the mutex protected code is not necessarily required/expected to
// complete quickly as no other sytem wide locks are held on account of an acquired mutex.
class Mutex : public MutexInterface
{
    public:
        Mutex();

        virtual ~Mutex();

        virtual void lock() final;

        virtual void unlock() final;

        virtual int8_t tryLock() final;

    private:
        mutex_t mMutex;
};

#endif // __MUTEX_H__
