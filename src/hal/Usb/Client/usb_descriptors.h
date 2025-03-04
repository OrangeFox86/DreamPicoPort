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

#ifndef __USB_DESCRITORS_H__
#define __USB_DESCRITORS_H__

#include "configuration.h"
#include <stdint.h>

#define GAMEPAD_MAIN_REPORT_ID 1
#define REPORT_ID_DC_RAW_DATA 2
#define GAMEPAD_REPORT_SIZE 64

#define ITF_NUM_GAMEPAD(idx) (idx)

#define MAX_NUMBER_OF_USB_GAMEPADS (4)

// For mass storage device
#define ITF_NUM_MSC (4)

#define ITF_NUM_CDC (5)
#define ITF_NUM_CDC_DATA (6)
#define ITF_COUNT(numGamepads) (numGamepads + 3)

//! Minumum analog value defined in USB HID descriptors
static const int8_t MIN_ANALOG_VALUE = -127;
//! Maximum analog value defined in USB HID descriptors
static const int8_t MAX_ANALOG_VALUE = 127;
//! Minimum trigger value defined in USB HID descriptors
static const int8_t MIN_TRIGGER_VALUE = MIN_ANALOG_VALUE;
//! Maximum trigger value defined in USB HID descriptors
static const int8_t MAX_TRIGGER_VALUE = MAX_ANALOG_VALUE;

#endif // __USB_DESCRITORS_H__
