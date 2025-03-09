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

#include "UsbGamepad.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "tusb.h"
#include "usb_descriptors.h"
#include "class/hid/hid.h"
#include "class/hid/hid_device.h"

#include "utils.h"

UsbGamepad::UsbGamepad(uint8_t playerIdx) :
  playerIdx(playerIdx),
  currentDpad(),
  currentButtons(0),
  buttonsUpdated(true)
{
  updateAllReleased();
}

bool UsbGamepad::isButtonPressed()
{
  return (
    currentDpad[DPAD_UP]
    || currentDpad[DPAD_DOWN]
    || currentDpad[DPAD_LEFT]
    || currentDpad[DPAD_RIGHT]
    || currentButtons != 0
    || isAnalogPressed(currentLeftAnalog[0])
    || isAnalogPressed(currentLeftAnalog[1])
    || isTriggerPressed(currentLeftAnalog[2])
    || isAnalogPressed(currentRightAnalog[0])
    || isAnalogPressed(currentRightAnalog[1])
    || isTriggerPressed(currentRightAnalog[2])
  );
}

//--------------------------------------------------------------------+
// EXTERNAL API
//--------------------------------------------------------------------+
void UsbGamepad::setAnalogThumbX(bool isLeft, int8_t x)
{
  x = limit_value(x, MIN_ANALOG_VALUE, MAX_ANALOG_VALUE);
  int8_t lastX = 0;
  if (isLeft)
  {
    lastX = currentLeftAnalog[0];
    currentLeftAnalog[0] = x;
  }
  else
  {
    lastX = currentRightAnalog[0];
    currentRightAnalog[0] = x;
  }
  buttonsUpdated = buttonsUpdated || (x != lastX);
}

void UsbGamepad::setAnalogThumbY(bool isLeft, int8_t y)
{
  y = limit_value(y, MIN_ANALOG_VALUE, MAX_ANALOG_VALUE);
  int8_t lastY = 0;
  if (isLeft)
  {
    lastY = currentLeftAnalog[1];
    currentLeftAnalog[1] = y;
  }
  else
  {
    lastY = currentRightAnalog[1];
    currentRightAnalog[1] = y;
  }
  buttonsUpdated = buttonsUpdated || (y != lastY);
}

void UsbGamepad::setAnalogTrigger(bool isLeft, int8_t z)
{
  z = limit_value(z, MIN_TRIGGER_VALUE, MAX_TRIGGER_VALUE);
  int8_t lastZ = 0;
  if (isLeft)
  {
    lastZ = currentLeftAnalog[2];
    currentLeftAnalog[2] = z;
  }
  else
  {
    lastZ = currentRightAnalog[2];
    currentRightAnalog[2] = z;
  }
  buttonsUpdated = buttonsUpdated || (z != lastZ);
}

int8_t UsbGamepad::getAnalogThumbX(bool isLeft)
{
  if (isLeft)
  {
    return currentLeftAnalog[0];
  }
  else
  {
    return currentRightAnalog[0];
  }
}

int8_t UsbGamepad::getAnalogThumbY(bool isLeft)
{
  if (isLeft)
  {
    return currentLeftAnalog[1];
  }
  else
  {
    return currentRightAnalog[1];
  }
}

int8_t UsbGamepad::getAnalogTrigger(bool isLeft)
{
  if (isLeft)
  {
    return currentLeftAnalog[2];
  }
  else
  {
    return currentRightAnalog[2];
  }
}

void UsbGamepad::setDigitalPad(UsbGamepad::DpadButtons button, bool isPressed)
{
  bool oldValue = currentDpad[button];
  currentDpad[button] = isPressed;
  buttonsUpdated = buttonsUpdated || (oldValue != currentDpad[button]);
}

void UsbGamepad::setButtonMask(uint32_t mask, bool isPressed)
{
  uint32_t lastButtons = currentButtons;
  if (isPressed)
  {
    currentButtons |= mask;
  }
  else
  {
    currentButtons &= ~mask;
  }
  buttonsUpdated = buttonsUpdated || (lastButtons != currentButtons);
}

void UsbGamepad::setButton(uint8_t button, bool isPressed)
{
  setButtonMask(1 << button, isPressed);
}

void UsbGamepad::updateAllReleased()
{
  if (isButtonPressed())
  {
    currentLeftAnalog[0] = 0;
    currentLeftAnalog[1] = 0;
    currentLeftAnalog[2] = MIN_TRIGGER_VALUE;
    currentRightAnalog[0] = 0;
    currentRightAnalog[1] = 0;
    currentRightAnalog[2] = MIN_TRIGGER_VALUE;
    currentDpad[DPAD_UP] = false;
    currentDpad[DPAD_DOWN] = false;
    currentDpad[DPAD_LEFT] = false;
    currentDpad[DPAD_RIGHT] = false;
    currentButtons = 0;
    buttonsUpdated = true;
  }
}

uint8_t UsbGamepad::getHatValue()
{
  if (currentDpad[DPAD_UP])
  {
    if (currentDpad[DPAD_LEFT])
    {
      return GAMEPAD_HAT_UP_LEFT;
    }
    else if (currentDpad[DPAD_RIGHT])
    {
      return GAMEPAD_HAT_UP_RIGHT;
    }
    else
    {
      return GAMEPAD_HAT_UP;
    }
  }
  else if (currentDpad[DPAD_DOWN])
  {
    if (currentDpad[DPAD_LEFT])
    {
      return GAMEPAD_HAT_DOWN_LEFT;
    }
    else if (currentDpad[DPAD_RIGHT])
    {
      return GAMEPAD_HAT_DOWN_RIGHT;
    }
    else
    {
      return GAMEPAD_HAT_DOWN;
    }
  }
  else if (currentDpad[DPAD_LEFT])
  {
    return GAMEPAD_HAT_LEFT;
  }
  else if (currentDpad[DPAD_RIGHT])
  {
    return GAMEPAD_HAT_RIGHT;
  }
  else
  {
    return GAMEPAD_HAT_CENTERED;
  }
}

bool UsbGamepad::send(bool force)
{
  if (buttonsUpdated || force)
  {
    bool sent = sendReport(ITF_NUM_GAMEPAD(playerIdx), GAMEPAD_MAIN_REPORT_ID);
    if (sent)
    {
      buttonsUpdated = false;
    }
    return sent;
  }
  else
  {
    return true;
  }
}

typedef struct TU_ATTR_PACKED
{
  int8_t  x;         ///< Delta x  movement of left analog-stick
  int8_t  y;         ///< Delta y  movement of left analog-stick
  int8_t  z;         ///< Delta z  movement of right analog-joystick
  int8_t  rz;        ///< Delta Rz movement of right analog-joystick
  int8_t  rx;        ///< Delta Rx movement of analog left trigger
  int8_t  ry;        ///< Delta Ry movement of analog right trigger
  uint8_t hat;       ///< Buttons mask for currently pressed buttons in the DPad/hat
  uint32_t buttons;  ///< Buttons mask for currently pressed buttons
  uint8_t pad;       ///< Vendor data (padding)
}hid_dc_gamepad_report_t;

uint8_t UsbGamepad::getReportSize()
{
  return sizeof(hid_dc_gamepad_report_t);
}

uint16_t UsbGamepad::getReport(uint8_t *buffer, uint16_t reqlen)
{
  // Build the report
  hid_dc_gamepad_report_t report;
  report.x = currentLeftAnalog[0];
  report.y = currentLeftAnalog[1];
  report.z = currentLeftAnalog[2];
  report.rz = currentRightAnalog[2];
  report.rx = currentRightAnalog[0];
  report.ry = currentRightAnalog[1];
  report.hat = getHatValue();
  report.buttons = currentButtons;
  report.pad = playerIdx; // Just put player index in this padding
  // Copy report into buffer
  uint16_t setLen = (sizeof(report) <= reqlen) ? sizeof(report) : reqlen;
  memcpy(buffer, &report, setLen);
  return setLen;
}
