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
#include "hal/Usb/usb_interface.hpp"
#include "hal/System/DppSettings.hpp"
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

#include <hardware/watchdog.h>

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

std::vector<uint8_t> get_controller_state(uint8_t idx)
{
  std::vector<uint8_t> report;
  if (idx < MAX_NUMBER_OF_USB_GAMEPADS)
  {
    report.resize(devices[idx]->getReportSize());
    devices[idx]->getReport(&report[0], static_cast<uint16_t>(report.size()));
  }
  return report;
}

bool usbEnabled = false;

bool usbDisconnecting = false;
absolute_time_t usbDisconnectTime;

bool gIsConnected = false;

int32_t gUsbLedGpio = -1;
int32_t gSimpleUsbLedGpio = -1;

void led_task()
{
  if (gUsbLedGpio >= 0)
  {
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
      UsbControllerDevice** pdevs = devices;
      for (uint32_t i = 0; i < MAX_NUMBER_OF_USB_GAMEPADS; ++i, ++pdevs)
      {
        if (is_usb_descriptor_gamepad_en(i))
        {
          if ((*pdevs)->isButtonPressed())
          {
            keyPressed = true;
            break;
          }
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
    gpio_put(gUsbLedGpio, ledOn);
  }

  if (gSimpleUsbLedGpio >= 0)
  {
    gpio_put(gSimpleUsbLedGpio, gIsConnected);
  }
}

void usb_init(
  MutexInterface* mscMutex,
  MutexInterface* cdcStdioMutex,
  MutexInterface* webUsbMutex,
  int32_t usbLedGpio,
  int32_t simpleUsbLedGpio
)
{
  board_init();
  msc_init(mscMutex);
  cdc_init(cdcStdioMutex);
  webusb_init(webUsbMutex);

  gUsbLedGpio = usbLedGpio;
  if (gUsbLedGpio >= 0)
  {
    gpio_init(gUsbLedGpio);
    gpio_set_dir_out_masked(1<<gUsbLedGpio);
  }

  gSimpleUsbLedGpio = simpleUsbLedGpio;
  if (gSimpleUsbLedGpio >= 0)
  {
    gpio_init(gSimpleUsbLedGpio);
    gpio_set_dir_out_masked(1<<gSimpleUsbLedGpio);
  }
}

void usb_start()
{
  dcd_connect(0);
  tusb_init();
}

void usb_stop()
{
  dcd_disconnect(0);
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
  UsbControllerDevice** pdevs = devices;
  for (uint32_t i = 0; i < MAX_NUMBER_OF_USB_GAMEPADS; ++i, ++pdevs)
  {
      if (is_usb_descriptor_gamepad_en(i))
      {
        (*pdevs)->updateUsbConnected(true);
        (*pdevs)->send(true);
      }
  }
  gIsConnected = true;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  UsbControllerDevice** pdevs = devices;
  for (uint32_t i = 0; i < MAX_NUMBER_OF_USB_GAMEPADS; ++i, ++pdevs)
  {
    if (is_usb_descriptor_gamepad_en(i))
    {
      (*pdevs)->updateUsbConnected(false);
    }
  }
  gIsConnected = false;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  UsbControllerDevice** pdevs = devices;
  for (uint32_t i = 0; i < MAX_NUMBER_OF_USB_GAMEPADS; ++i, ++pdevs)
  {
    if (is_usb_descriptor_gamepad_en(i))
    {
      (*pdevs)->updateUsbConnected(false);
    }
  }
  gIsConnected = false;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  UsbControllerDevice** pdevs = devices;
  for (uint32_t i = 0; i < MAX_NUMBER_OF_USB_GAMEPADS; ++i, ++pdevs)
  {
    if (is_usb_descriptor_gamepad_en(i))
    {
      (*pdevs)->updateUsbConnected(true);
      (*pdevs)->send(true);
    }
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

  int16_t idx = usb_gamepad_instance_to_index(instance);
  if (idx < 0)
  {
    return 0;
  }

  // Build the report for the given report ID and return the size set
  return devices[idx]->getReport(buffer, reqlen);
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
  webusb_rx(itf, buffer, bufsize);

  // if using RX buffered is enabled, we need to flush the buffer to make room for new data
  #if CFG_TUD_VENDOR_RX_BUFSIZE > 0
  tud_vendor_read_flush();
  #endif
}

static bool webusb_link_announce_enable = true;
void usb_webusb_link_announce_enable(bool enabled)
{
  webusb_link_announce_enable = enabled;
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
          const std::uint8_t headerLen = 3;
          const std::uint8_t bufLen = headerLen + strlen(webusb_url);
          std::uint8_t buffer[bufLen];
          buffer[0] = bufLen;
          buffer[1] = 3; // WEBUSB URL type
          buffer[2] = 1; // 0: http, 1: https
          if (webusb_link_announce_enable)
          {
            memcpy(&buffer[3], webusb_url, strlen(webusb_url));
            return tud_control_xfer(rhport, request, buffer, bufLen);
          }
          else
          {
            return tud_control_xfer(rhport, request, buffer, headerLen);
          }
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
        printf("index %i val %i", (int)request->wIndex, (int)request->wValue);
        if (request->wValue == 0xFFFF)
        {
          watchdog_hw->scratch[0] = WATCHDOG_SETTINGS_USB_REBOOT;
          // Special request: cause reboot in 250 ms
          watchdog_reboot(0, 0, 250);
        }
        else if (request->wValue == 0xA5A5)
        {
          // Special request: reset cdc parser buffers
          usb_cdc_reset_parser_buffers();
        }
        else
        {
          // Webserial simulate the CDC_REQUEST_SET_CONTROL_LINE_STATE (0x22) to connect and disconnect.
          webusb_connection_event(request->wIndex, request->wValue);
        }

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

