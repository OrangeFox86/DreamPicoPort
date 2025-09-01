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

#ifndef __USB_INTERFACE_H__
#define __USB_INTERFACE_H__

#include "UsbFileSystem.hpp"
#include "DreamcastControllerObserver.hpp"
#include "hal/System/MutexInterface.hpp"
#include <vector>

static const uint32_t WATCHDOG_SETTINGS_USB_REBOOT = 0x4660DFDA;

//! @returns array of the USB controller observers
DreamcastControllerObserver** get_usb_controller_observers();
//! USB initialization
void usb_init(
  MutexInterface* mscMutex,
  MutexInterface* cdcStdioMutex,
  MutexInterface* webUsbMutex,
  int32_t usbLedGpio,
  int32_t simpleUsbLedGpio
);
//! Start USB execution
void usb_start();
//! Stop USB execution
void usb_stop();
//! USB task that needs to be called constantly by main()
void usb_task();

//! Must return the file system
UsbFileSystem& usb_msc_get_file_system();

//! Get the controller state for a controller at \p idx
//! @param[in] idx The controller index
//! @return controller state or empty vector if idx is invalid
std::vector<uint8_t> get_controller_state(uint8_t idx);

//! Enable or disable WebUSB announcement link
void usb_webusb_link_announce_enable(bool enabled);

#endif // __USB_INTERFACE_H__
