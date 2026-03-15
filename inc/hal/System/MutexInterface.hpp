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

#ifndef __MUTEX_INTERFACE_H__
#define __MUTEX_INTERFACE_H__

#include <stdint.h>

//! This interface is used to decouple mutex functionality in HAL from the Dreamcast functionality.
class MutexInterface
{
    public:
        //! Constructor
        MutexInterface() {}

        //! Virtual destructor
        virtual ~MutexInterface() {}

        //! Blocks until mutex is available and then takes it
        virtual void lock() = 0;

        //! Releases the previously locked mutex
        virtual void unlock() = 0;

        //! Tries to obtain mutex without blocking
        //! @returns 1 if mutex was obtained
        //! @returns 0 if mutex was not obtained, and blocking would be valid in this context
        //! @returns -1 if mutex was not obtained, and blocking would cause deadlock
        virtual int8_t tryLock() = 0;
};

#endif // __MUTEX_INTERFACE_H__
