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

#ifndef __DREAMCAST_CONTROLLER_OBSERVER_H__
#define __DREAMCAST_CONTROLLER_OBSERVER_H__

#include <stdint.h>
#include "dreamcast_structures.h"

//! This interface is used to decouple the USB functionality in HAL from the Dreamcast functionality
class DreamcastControllerObserver
{
    public:
        typedef controller_condition_t ControllerCondition;
        typedef vmu_timer_condition_t SecondaryControllerCondition;

        //! Sets the current Dreamcast controller condition
        //! @param[in] controllerCondition  The current condition of the Dreamcast controller
        virtual void setControllerCondition(const ControllerCondition& controllerCondition) = 0;

        //! Sets the current Dreamcast secondary controller condition
        //! @param[in] secondaryControllerCondition  The current secondary condition of the
        //!                                          Dreamcast controller
        virtual void setSecondaryControllerCondition(
            const SecondaryControllerCondition& secondaryControllerCondition) = 0;

        //! Sets the change condition signal
        //! @param[in] changeSignal  Send true then false about 25 ms later on change
        virtual void setChangeCondition(bool changeSignal) = 0;

        //! Called when controller connected
        virtual void controllerConnected() = 0;

        //! Called when controller disconnected
        virtual void controllerDisconnected() = 0;

        //! Set the instance ID for sending report
        //! @param[in] instance The instance ID
        virtual void setInstanceId(uint8_t instance) = 0;

        //! Force send of data over the HID interface
        virtual void forceSend() = 0;
};

#endif // __DREAMCAST_CONTROLLER_OBSERVER_H__
