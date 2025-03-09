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

#include "UsbControllerDevice.h"
#include <stdint.h>
#include "class/hid/hid_device.h"

UsbControllerDevice::UsbControllerDevice() : mIsUsbConnected(false), mIsControllerConnected(false) {}
UsbControllerDevice::~UsbControllerDevice() {}

void UsbControllerDevice::updateUsbConnected(bool connected)
{
  mIsUsbConnected = connected;
}

bool UsbControllerDevice::isUsbConnected()
{
  return mIsUsbConnected;
}

void UsbControllerDevice::updateControllerConnected(bool connected)
{
  if (connected != mIsControllerConnected)
  {
    updateAllReleased();
  }
  mIsControllerConnected = connected;
}

bool UsbControllerDevice::isControllerConnected()
{
  return mIsControllerConnected;
}

bool UsbControllerDevice::sendReport(uint8_t instance, uint8_t report_id)
{
  bool sent = false;
  if (isUsbConnected())
  {
    uint8_t reportSize = getReportSize();
    uint8_t reportBuffer[reportSize];
    getReport(reportBuffer, reportSize);
    sent = tud_hid_n_report(instance, report_id, reportBuffer, reportSize);
  }
  return sent;
}
