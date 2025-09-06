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

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void set_usb_descriptor_gamepad_en(uint8_t idx, bool en);
bool is_usb_descriptor_gamepad_en(uint8_t idx);
int16_t usb_gamepad_instance_to_index(uint8_t instance);

//! Enables or disables the USB CDC (Communication Data Class aka "serial" or "tty") interface
void set_usb_cdc_en(bool en);
//! @return true iff USB CDC is enabled
bool is_usb_cdc_en();
//! Enables or disables the USB MSC (Mass Storage Class) interface
void set_usb_msc_en(bool en);
//! @return true iff USB MSC is enabled
bool is_usb_msc_en();

#ifdef __cplusplus
}
#endif
