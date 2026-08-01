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

#include "UsbGamepadDreamcastControllerObserver.hpp"

UsbGamepadDreamcastControllerObserver::UsbGamepadDreamcastControllerObserver(UsbGamepad& usbController) :
    mUsbController(usbController),
    mDpadType(DpadType::HAT),
    mNextSendForced(false),
    mSendPending(false)
{}

void UsbGamepadDreamcastControllerObserver::setControllerCondition(const ControllerCondition& controllerCondition)
{
    mUsbController.setButton(DREAMCAST_GAMEPAD_BUTTON_A, 0 == controllerCondition.a);
    mUsbController.setButton(DREAMCAST_GAMEPAD_BUTTON_B, 0 == controllerCondition.b);
    mUsbController.setButton(DREAMCAST_GAMEPAD_BUTTON_C, 0 == controllerCondition.c);
    mUsbController.setButton(DREAMCAST_GAMEPAD_BUTTON_X, 0 == controllerCondition.x);
    mUsbController.setButton(DREAMCAST_GAMEPAD_BUTTON_Y, 0 == controllerCondition.y);
    mUsbController.setButton(DREAMCAST_GAMEPAD_BUTTON_Z, 0 == controllerCondition.z);
    mUsbController.setButton(DREAMCAST_GAMEPAD_BUTTON_START, 0 == controllerCondition.start);

    // Mapping these to random unique buttons just in case something out there uses them
    mUsbController.setButton(DREAMCAST_GAMEPAD_RIGHT_B, 0 == controllerCondition.rightb);
    mUsbController.setButton(DREAMCAST_GAMEPAD_LEFT_B, 0 == controllerCondition.leftb);
    mUsbController.setButton(DREAMCAST_GAMEPAD_DOWN_B, 0 == controllerCondition.downb);
    mUsbController.setButton(DREAMCAST_GAMEPAD_UP_B, 0 == controllerCondition.upb);
    mUsbController.setButton(DREAMCAST_GAMEPAD_BUTTON_D, 0 == controllerCondition.d);

    switch (mDpadType)
    {
        case DpadType::BUTTONS:
        {
            mUsbController.setDigitalPad(UsbGamepad::DPAD_UP, false);
            mUsbController.setDigitalPad(UsbGamepad::DPAD_DOWN, false);
            mUsbController.setDigitalPad(UsbGamepad::DPAD_LEFT, false);
            mUsbController.setDigitalPad(UsbGamepad::DPAD_RIGHT, false);

            mUsbController.setButton(DREAMCAST_GAMEPAD_ALT_UP, 0 == controllerCondition.up);
            mUsbController.setButton(DREAMCAST_GAMEPAD_ALT_DOWN, 0 == controllerCondition.down);
            mUsbController.setButton(DREAMCAST_GAMEPAD_ALT_LEFT, 0 == controllerCondition.left);
            mUsbController.setButton(DREAMCAST_GAMEPAD_ALT_RIGHT, 0 == controllerCondition.right);
        }
        break;

        case DpadType::BOTH:
        {
            mUsbController.setDigitalPad(UsbGamepad::DPAD_UP, 0 == controllerCondition.up);
            mUsbController.setDigitalPad(UsbGamepad::DPAD_DOWN, 0 == controllerCondition.down);
            mUsbController.setDigitalPad(UsbGamepad::DPAD_LEFT, 0 == controllerCondition.left);
            mUsbController.setDigitalPad(UsbGamepad::DPAD_RIGHT, 0 == controllerCondition.right);

            mUsbController.setButton(DREAMCAST_GAMEPAD_ALT_UP, 0 == controllerCondition.up);
            mUsbController.setButton(DREAMCAST_GAMEPAD_ALT_DOWN, 0 == controllerCondition.down);
            mUsbController.setButton(DREAMCAST_GAMEPAD_ALT_LEFT, 0 == controllerCondition.left);
            mUsbController.setButton(DREAMCAST_GAMEPAD_ALT_RIGHT, 0 == controllerCondition.right);
        }
        break;

        case DpadType::HAT: // Fall through
        default:
        {
            mUsbController.setDigitalPad(UsbGamepad::DPAD_UP, 0 == controllerCondition.up);
            mUsbController.setDigitalPad(UsbGamepad::DPAD_DOWN, 0 == controllerCondition.down);
            mUsbController.setDigitalPad(UsbGamepad::DPAD_LEFT, 0 == controllerCondition.left);
            mUsbController.setDigitalPad(UsbGamepad::DPAD_RIGHT, 0 == controllerCondition.right);

            mUsbController.setButton(DREAMCAST_GAMEPAD_ALT_UP, false);
            mUsbController.setButton(DREAMCAST_GAMEPAD_ALT_DOWN, false);
            mUsbController.setButton(DREAMCAST_GAMEPAD_ALT_LEFT, false);
            mUsbController.setButton(DREAMCAST_GAMEPAD_ALT_RIGHT, false);
        }
        break;
    }

    mUsbController.setAnalogTrigger(true, static_cast<int32_t>(controllerCondition.l) - 128);
    mUsbController.setAnalogTrigger(false, static_cast<int32_t>(controllerCondition.r) - 128);

    mUsbController.setAnalogThumbX(true, static_cast<int32_t>(controllerCondition.lAnalogLR) - 128);
    mUsbController.setAnalogThumbY(true, static_cast<int32_t>(controllerCondition.lAnalogUD) - 128);
    mUsbController.setAnalogThumbX(false, static_cast<int32_t>(controllerCondition.rAnalogLR) - 128);
    mUsbController.setAnalogThumbY(false, static_cast<int32_t>(controllerCondition.rAnalogUD) - 128);

    mSendPending.store(true, std::memory_order_release);
}

void UsbGamepadDreamcastControllerObserver::setSecondaryControllerCondition(
    const SecondaryControllerCondition& secondaryControllerCondition)
{
    mUsbController.setButton(DREAMCAST_VMU1_BUTTON_A, 0 == secondaryControllerCondition.a);
    mUsbController.setButton(DREAMCAST_VMU1_BUTTON_B, 0 == secondaryControllerCondition.b);
    mUsbController.setButton(DREAMCAST_VMU1_BUTTON_UP, 0 == secondaryControllerCondition.up);
    mUsbController.setButton(DREAMCAST_VMU1_BUTTON_DOWN, 0 == secondaryControllerCondition.down);
    mUsbController.setButton(DREAMCAST_VMU1_BUTTON_LEFT, 0 == secondaryControllerCondition.left);
    mUsbController.setButton(DREAMCAST_VMU1_BUTTON_RIGHT, 0 == secondaryControllerCondition.right);

    // Don't bother USB with this update - only update within setControllerCondition()
    //mSendPending.store(true, std::memory_order_release);
}

void UsbGamepadDreamcastControllerObserver::setChangeCondition(bool changeSignal)
{
    mUsbController.setButton(DREAMCAST_GAMEPAD_CHANGE_EVENT, changeSignal);
    mSendPending.store(true, std::memory_order_release);
}

void UsbGamepadDreamcastControllerObserver::controllerConnected()
{
    mUsbController.updateControllerConnected(true);
    mNextSendForced.store(true, std::memory_order_relaxed);
    mSendPending.store(true, std::memory_order_release);
}

void UsbGamepadDreamcastControllerObserver::controllerDisconnected()
{
    mUsbController.updateControllerConnected(false);
    mNextSendForced.store(true, std::memory_order_relaxed);
    mSendPending.store(true, std::memory_order_release);
}

void UsbGamepadDreamcastControllerObserver::setInstanceId(uint8_t instance)
{
    mUsbController.setInstanceId(instance);
}

void UsbGamepadDreamcastControllerObserver::forceSend()
{
    mNextSendForced.store(true, std::memory_order_relaxed);
    mSendPending.store(true, std::memory_order_release);
}

void UsbGamepadDreamcastControllerObserver::setDpadOutput(DpadType dpadType)
{
    mDpadType = dpadType;
}

void UsbGamepadDreamcastControllerObserver::process()
{
    if (mSendPending.exchange(false, std::memory_order_acquire))
    {
        const bool force = mNextSendForced.exchange(false, std::memory_order_relaxed);
        mUsbController.send(force);
    }
}
