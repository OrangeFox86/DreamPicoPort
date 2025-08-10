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
#include "UsbGamepadDreamcastControllerObserver.hpp"
#include "UsbGamepad.h"
#include "configuration.h"
#include "hal/Usb/client_usb_interface.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <cstdint>

#include "bsp/board.h"
#include "pico/stdlib.h"
#include "tusb.h"
#include "device/dcd.h"
#include "usb_descriptors.h"
#include "class/hid/hid_device.h"
#include "msc_disk.hpp"
#include "cdc.hpp"
#include "webusb.hpp"

UsbGamepad usbGamepads[MAX_NUMBER_OF_USB_GAMEPADS] = {
  UsbGamepad(0),
  UsbGamepad(1),
  UsbGamepad(2),
  UsbGamepad(3)
};

UsbGamepadDreamcastControllerObserver usbGamepadDreamcastControllerObservers[MAX_NUMBER_OF_USB_GAMEPADS] = {
  UsbGamepadDreamcastControllerObserver(usbGamepads[0]),
  UsbGamepadDreamcastControllerObserver(usbGamepads[1]),
  UsbGamepadDreamcastControllerObserver(usbGamepads[2]),
  UsbGamepadDreamcastControllerObserver(usbGamepads[3])
};

UsbControllerDevice* devices[MAX_NUMBER_OF_USB_GAMEPADS] = {
  &usbGamepads[0],
  &usbGamepads[1],
  &usbGamepads[2],
  &usbGamepads[3]
};

DreamcastControllerObserver* observers[MAX_NUMBER_OF_USB_GAMEPADS] = {
  &usbGamepadDreamcastControllerObservers[0],
  &usbGamepadDreamcastControllerObservers[1],
  &usbGamepadDreamcastControllerObservers[2],
  &usbGamepadDreamcastControllerObservers[3]
};

DreamcastControllerObserver** get_usb_controller_observers()
{
  return observers;
}

uint32_t get_num_usb_controllers()
{
  uint8_t installedGamepads = get_usb_descriptor_number_of_gamepads();

  if (installedGamepads <= MAX_NUMBER_OF_USB_GAMEPADS)
  {
    return installedGamepads;
  }
  else
  {
    return MAX_NUMBER_OF_USB_GAMEPADS;
  }
}

bool usbEnabled = false;

UsbControllerDevice** pAllUsbDevices = nullptr;

uint8_t numUsbDevices = 0;

bool usbDisconnecting = false;
absolute_time_t usbDisconnectTime;

void set_usb_devices(UsbControllerDevice** devices, uint8_t n)
{
  pAllUsbDevices = devices;
  numUsbDevices = n;
}

bool gIsConnected = false;

void led_task()
{
#if USB_LED_PIN >= 0
  static bool ledOn = false;
  static uint32_t startMs = 0;
  if (usbDisconnecting)
  {
    // Currently counting down to disconnect; flash LED
    static const uint32_t BLINK_TIME_MS = 500;
    uint32_t t = board_millis() - startMs;
    if (t >= BLINK_TIME_MS)
    {
      startMs += BLINK_TIME_MS;
      ledOn = !ledOn;
    }
  }
  else
  {
    bool keyPressed = false;
    UsbControllerDevice** pdevs = pAllUsbDevices;
    for (uint32_t i = numUsbDevices; i > 0; --i, ++pdevs)
    {
      if ((*pdevs)->isButtonPressed())
      {
        keyPressed = true;
        break;
      }
    }
    if (gIsConnected)
    {
      // When connected, LED is ON only when no key is pressed
      ledOn = !keyPressed;
    }
    else
    {
      // When not connected, LED blinks when key is pressed
      static const uint32_t BLINK_TIME_MS = 100;
      uint32_t t = board_millis() - startMs;
      if (t >= BLINK_TIME_MS)
      {
        startMs += BLINK_TIME_MS;
        ledOn = !ledOn;
      }
      ledOn = ledOn && keyPressed;
    }
  }
  gpio_put(USB_LED_PIN, ledOn);
#endif

#if SIMPLE_USB_LED_PIN >= 0
  gpio_put(SIMPLE_USB_LED_PIN, gIsConnected);
#endif

}

void usb_init(
  MutexInterface* mscMutex,
  MutexInterface* cdcStdioMutex,
  MutexInterface* webUsbMutex
)
{
  uint32_t numDevices = get_num_usb_controllers();

  uint32_t max = sizeof(devices) / sizeof(devices[1]);
  if (numDevices > max)
  {
    numDevices = max;
  }
  set_usb_devices(devices, numDevices);

  board_init();
  tusb_init();
  msc_init(mscMutex);
  cdc_init(cdcStdioMutex);
  webusb_init(webUsbMutex);

#if USB_LED_PIN >= 0
  gpio_init(USB_LED_PIN);
  gpio_set_dir_out_masked(1<<USB_LED_PIN);
#endif

#if SIMPLE_USB_LED_PIN >= 0
  gpio_init(SIMPLE_USB_LED_PIN);
  gpio_set_dir_out_masked(1<<SIMPLE_USB_LED_PIN);
#endif
}

void usb_task()
{
  tud_task(); // tinyusb device task
  led_task();
  cdc_task();
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  UsbControllerDevice** pdevs = pAllUsbDevices;
  for (uint32_t i = numUsbDevices; i > 0; --i, ++pdevs)
  {
    (*pdevs)->updateUsbConnected(true);
    (*pdevs)->send(true);
  }
  gIsConnected = true;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  UsbControllerDevice** pdevs = pAllUsbDevices;
  for (uint32_t i = numUsbDevices; i > 0; --i, ++pdevs)
  {
    (*pdevs)->updateUsbConnected(false);
  }
  gIsConnected = false;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  UsbControllerDevice** pdevs = pAllUsbDevices;
  for (uint32_t i = numUsbDevices; i > 0; --i, ++pdevs)
  {
    (*pdevs)->updateUsbConnected(false);
  }
  gIsConnected = false;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  UsbControllerDevice** pdevs = pAllUsbDevices;
  for (uint32_t i = numUsbDevices; i > 0; --i, ++pdevs)
  {
    (*pdevs)->updateUsbConnected(true);
    (*pdevs)->send(true);
  }
  gIsConnected = true;
}

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
  (void) report_id;
  (void) report_type;
  if (instance >= numUsbDevices)
  {
    return 0;
  }
  else
  {
    // Build the report for the given report ID and return the size set
    return pAllUsbDevices[instance]->getReport(buffer, reqlen);
  }
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance,
                           uint8_t report_id,
                           hid_report_type_t report_type,
                           uint8_t const *buffer,
                           uint16_t bufsize)
{
  (void) report_type;

  // echo back anything we received from host
  tud_hid_n_report(instance, report_id, buffer, bufsize);
}

//--------------------------------------------------------------------+
// USB Vendor (WebUSB/WinUSB)
//--------------------------------------------------------------------+

void tud_vendor_rx_cb(uint8_t itf, uint8_t const* buffer, uint16_t bufsize)
{
  webusb_process(itf, buffer, bufsize);

  // if using RX buffered is enabled, we need to flush the buffer to make room for new data
  #if CFG_TUD_VENDOR_RX_BUFSIZE > 0
  tud_vendor_read_flush();
  #endif
}

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request)
{
  // nothing to with DATA & ACK stage
  if (stage != CONTROL_STAGE_SETUP) return true;

  switch (request->bmRequestType_bit.type) {
    case TUSB_REQ_TYPE_VENDOR:
      switch (request->bRequest) {
        case VENDOR_REQUEST_WEBUSB:
        {
          // match vendor request in BOS descriptor
          // Get landing page url
          const std::uint8_t bufLen = 3 + strlen(webusb_url);
          std::uint8_t buffer[bufLen];
          buffer[0] = bufLen;
          buffer[1] = 3; // WEBUSB URL type
          buffer[2] = 1; // 0: http, 1: https
          memcpy(&buffer[3], webusb_url, strlen(webusb_url));
          return tud_control_xfer(rhport, request, buffer, bufLen);
        }

        case VENDOR_REQUEST_MICROSOFT:
          if (request->wIndex == 7) {
            // Get Microsoft OS 2.0 compatible descriptor
            uint16_t total_len;
            memcpy(&total_len, desc_ms_os_20 + 8, 2);

            return tud_control_xfer(rhport, request, (void*)(uintptr_t)desc_ms_os_20, total_len);
          } else {
            return false;
          }

        default: break;
      }
      break;

    case TUSB_REQ_TYPE_CLASS:
      if (request->bRequest == 0x22) {
        // Webserial simulate the CDC_REQUEST_SET_CONTROL_LINE_STATE (0x22) to connect and disconnect.
        webusb_connection_event(request->wIndex, request->wValue);

        // response with status OK
        return tud_control_status(rhport, request);
      }
      break;

    default: break;
  }

  // stall unknown request
  return false;
}

static bool cdc_en = false;

void set_usb_cdc_en(bool en)
{
    cdc_en = en;
}

bool is_usb_cdc_en()
{
    return cdc_en;
}

static bool msc_en = false;

void set_usb_msc_en(bool en)
{
    msc_en = en;
}

bool is_usb_msc_en()
{
    return msc_en;
}

