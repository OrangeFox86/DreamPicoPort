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

#ifndef __LOCK_GUARD_H__
#define __LOCK_GUARD_H__

#include <stdint.h>
#include "MutexInterface.hpp"

//! This class is similar to std::lock_guard, but it is specifically meant to be used with
//! MutexInterface. The special thing with MutexInterface is that doing a blocking lock has the
//! potential to cause a deadlock (because spin lock is used, and there is no way to yield from IRQ
//! context). As such, the functionality using the mutex must check this and perform necessary
//! operations (even if it's just to log an error and spin forever).
class LockGuard
{
    public:
        LockGuard() = delete;

        //! Initializes data and locks mutex as long as it wouldn't cause a deadlock
        inline LockGuard(MutexInterface& mutex, bool allowDeadlock = false) :
            mMutex(mutex),
            mIsLocked(false)
        {
            if (allowDeadlock)
            {
                mMutex.lock();
                mIsLocked = true;
            }
            else
            {
                int8_t lockValue = mMutex.tryLock();
                if (lockValue == 0)
                {
                    mMutex.lock();
                    mIsLocked = true;
                }
                else if (lockValue > 0)
                {
                    mIsLocked = true;
                }
                // Else: not locked, would cause deadlock - due to simultaneous IRQ access on same core
            }
        }

        //! Unlocks mutex if it was previously locked
        virtual inline ~LockGuard()
        {
            if (mIsLocked)
            {
                mMutex.unlock();
            }
        }

        //! @returns true iff the mutex was successfully locked in the constructor or false if
        //!          locking would cause a deadlock
        inline bool isLocked()
        {
            return mIsLocked;
        }

    private:
        MutexInterface& mMutex;
        bool mIsLocked;
};

#endif // __LOCK_GUARD_H__
