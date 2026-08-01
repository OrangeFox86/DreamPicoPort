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

#ifndef __USB_CONTROLLER_DREAMCAST_CONTROLLER_OBSERVER_H__
#define __USB_CONTROLLER_DREAMCAST_CONTROLLER_OBSERVER_H__

#include "hal/Usb/DreamcastControllerObserver.hpp"
#include "UsbGamepad.h"

#include <atomic>

//! Yes, I know this name is ridiculous, but at least it's descriptive!
//! This connects the Dreamcast controller observer to a USB gamepad device
class UsbGamepadDreamcastControllerObserver : public DreamcastControllerObserver
{
    public:
        //! Enumerates the locally assigned buttons
        enum DreamcastGamepadButton : uint8_t
        {
            DREAMCAST_GAMEPAD_BUTTON_A = UsbGamepad::GAMEPAD_BUTTON_A,
            DREAMCAST_GAMEPAD_BUTTON_B = UsbGamepad::GAMEPAD_BUTTON_B,
            DREAMCAST_GAMEPAD_BUTTON_C = UsbGamepad::GAMEPAD_BUTTON_C,
            DREAMCAST_GAMEPAD_BUTTON_X = UsbGamepad::GAMEPAD_BUTTON_X,
            DREAMCAST_GAMEPAD_BUTTON_Y = UsbGamepad::GAMEPAD_BUTTON_Y,
            DREAMCAST_GAMEPAD_BUTTON_Z = UsbGamepad::GAMEPAD_BUTTON_Z,
            DREAMCAST_GAMEPAD_RIGHT_B = UsbGamepad::GAMEPAD_BUTTON_TL,
            DREAMCAST_GAMEPAD_LEFT_B = UsbGamepad::GAMEPAD_BUTTON_TR,
            DREAMCAST_GAMEPAD_DOWN_B = UsbGamepad::GAMEPAD_BUTTON_TL2,
            DREAMCAST_GAMEPAD_UP_B = UsbGamepad::GAMEPAD_BUTTON_TR2,
            DREAMCAST_GAMEPAD_BUTTON_D = UsbGamepad::GAMEPAD_BUTTON_SELECT,
            DREAMCAST_GAMEPAD_BUTTON_START = UsbGamepad::GAMEPAD_BUTTON_START,
            DREAMCAST_VMU1_BUTTON_A = UsbGamepad::GAMEPAD_BUTTON_MODE,
            // UNASSIGNED = UsbGamepad::GAMEPAD_BUTTON_THUMBL,
            // UNASSIGNED = UsbGamepad::GAMEPAD_BUTTON_THUMBR,
            DREAMCAST_VMU1_BUTTON_B = UsbGamepad::BUTTON15,
            DREAMCAST_VMU1_BUTTON_UP = UsbGamepad::BUTTON16,
            DREAMCAST_VMU1_BUTTON_DOWN = UsbGamepad::BUTTON17,
            DREAMCAST_VMU1_BUTTON_LEFT = UsbGamepad::BUTTON18,
            DREAMCAST_VMU1_BUTTON_RIGHT = UsbGamepad::BUTTON19,
            DREAMCAST_GAMEPAD_CHANGE_EVENT = UsbGamepad::BUTTON20,
            // UNASSIGNED = UsbGamepad::BUTTON21,
            // UNASSIGNED = UsbGamepad::BUTTON22,
            // UNASSIGNED = UsbGamepad::BUTTON23,
            DREAMCAST_GAMEPAD_ALT_UP = UsbGamepad::BUTTON24,
            DREAMCAST_GAMEPAD_ALT_DOWN = UsbGamepad::BUTTON25,
            DREAMCAST_GAMEPAD_ALT_LEFT = UsbGamepad::BUTTON26,
            DREAMCAST_GAMEPAD_ALT_RIGHT = UsbGamepad::BUTTON27,
            DREAMCAST_GAMEPAD_PLAYER_4 = UsbGamepad::BUTTON28,
            DREAMCAST_GAMEPAD_PLAYER_3 = UsbGamepad::BUTTON29,
            DREAMCAST_GAMEPAD_PLAYER_2 = UsbGamepad::BUTTON30,
            DREAMCAST_GAMEPAD_PLAYER_1 = UsbGamepad::BUTTON31
        };

        //! Constructor for UsbKeyboardGenesisControllerObserver
        //! @param[in] usbController  The USB controller to update when keys are pressed or released
        UsbGamepadDreamcastControllerObserver(UsbGamepad& usbController);

        //! Sets the current Dreamcast controller condition
        //! @param[in] controllerCondition  The current condition of the Dreamcast controller
        void setControllerCondition(const ControllerCondition& controllerCondition) override final;

        //! Sets the current Dreamcast secondary controller condition
        //! @param[in] secondaryControllerCondition  The current secondary condition of the Dreamcast controller
        void setSecondaryControllerCondition(
            const SecondaryControllerCondition& secondaryControllerCondition
        ) override final;

        void setChangeCondition(bool changeSignal) override final;

        //! Called when controller connected
        void controllerConnected() final;

        //! Called when controller disconnected
        void controllerDisconnected() final;

        //! Set the instance ID for sending report
        //! @param[in] instance The instance ID
        void setInstanceId(uint8_t instance) override final;

        //! Force send of data over the HID interface
        void forceSend() override final;

        //! Sets the type of output for the D-Pad
        //! @param[in] dpadType the D-Pad output type
        void setDpadOutput(DpadType dpadType) override final;

        //! Send any waiting data to USB
        void process() override final;

    private:
        //! The USB controller I update
        UsbGamepad& mUsbController;
        //! The type of output for the D-Pad
        DpadType mDpadType;
        //! Set to true when the next send should force controller update
        std::atomic<bool> mNextSendForced;
        //! Set to true when a send is pending, handled on process()
        std::atomic<bool> mSendPending;
};

#endif // __USB_CONTROLLER_DREAMCAST_CONTROLLER_OBSERVER_H__
