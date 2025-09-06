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

#ifndef __USB_CONTROLLER_DREAMCAST_CONTROLLER_OBSERVER_H__
#define __USB_CONTROLLER_DREAMCAST_CONTROLLER_OBSERVER_H__

#include "hal/Usb/DreamcastControllerObserver.hpp"
#include "UsbGamepad.h"

//! Yes, I know this name is ridiculous, but at least it's descriptive!
//! This connects the Dreamcast controller observer to a USB gamepad device
class UsbGamepadDreamcastControllerObserver : public DreamcastControllerObserver
{
    public:
        //! Constructor for UsbKeyboardGenesisControllerObserver
        //! @param[in] usbController  The USB controller to update when keys are pressed or released
        UsbGamepadDreamcastControllerObserver(UsbGamepad& usbController);

        //! Sets the current Dreamcast controller condition
        //! @param[in] controllerCondition  The current condition of the Dreamcast controller
        virtual void setControllerCondition(const ControllerCondition& controllerCondition) final;

        //! Sets the current Dreamcast secondary controller condition
        //! @param[in] secondaryControllerCondition  The current secondary condition of the
        //!                                          Dreamcast controller
        virtual void setSecondaryControllerCondition(
            const SecondaryControllerCondition& secondaryControllerCondition) final;

        virtual void setChangeCondition(bool changeSignal) override final;

        //! Called when controller connected
        virtual void controllerConnected() final;

        //! Called when controller disconnected
        virtual void controllerDisconnected() final;

        //! Set the instance ID for sending report
        //! @param[in] instance The instance ID
        virtual void setInstanceId(uint8_t instance) override final;

        //! Force send of data over the HID interface
        virtual void forceSend() override final;

    private:
        //! The USB controller I update
        UsbGamepad& mUsbController;
};

#endif // __USB_CONTROLLER_DREAMCAST_CONTROLLER_OBSERVER_H__
