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

#include "cdc.hpp"

#include "UsbControllerDevice.h"
#include "UsbGamepadDreamcastControllerObserver.hpp"
#include "UsbGamepad.h"
#include "configuration.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board.h"
#include "pico/stdlib.h"
#include "tusb.h"
#include "device/dcd.h"
#include "usb_descriptors.h"
#include "class/hid/hid_device.h"
#include "class/cdc/cdc_device.h"

#include <hal/Usb/TtyParser.hpp>
#include <hal/Usb/client_usb_interface.h>

static bool echoOn = true;
static TtyParser* ttyParser = nullptr;

void usb_cdc_set_parser(TtyParser* parser)
{
    ttyParser = parser;
}

void usb_cdc_reset_parser_buffers()
{
    if (ttyParser)
    {
        ttyParser->reset();
    }
}

void usb_cdc_set_echo(bool on)
{
    echoOn = on;
}

void usb_cdc_write(const char *buf, int length)
{
#if CFG_TUD_CDC
    tud_cdc_write(buf, length);
    tud_task();
    tud_cdc_write_flush();
#endif
}


#if CFG_TUD_CDC

static MutexInterface* stdioMutex = nullptr;

// Can't use stdio_usb_init() because it checks tud_cdc_connected(), and that doesn't always return
// true when a connection is made. Not all terminal client set this when making connection.

#include "pico/stdio.h"
#include "pico/stdio/driver.h"
extern "C" {

static void stdio_usb_out_chars2(const char *buf, int length)
{
    if (length <= 0) return;

    static uint64_t last_avail_time;

    LockGuard lockGuard(*stdioMutex);
    if (!lockGuard.isLocked())
    {
        return; // would deadlock otherwise
    }

    for (uint32_t i = 0; i < (uint32_t)length;)
    {
        uint32_t n = length - i;
        uint32_t avail = tud_cdc_write_available();

        if (n > avail)
        {
            n = (int)avail;
        }

        if (n)
        {
            uint32_t n2 = tud_cdc_write(buf + i, n);
            tud_task();
            tud_cdc_write_flush();
            i += n2;
            last_avail_time = time_us_64();
        }
        else
        {
            tud_task();
            tud_cdc_write_flush();
            if (!tud_cdc_connected() ||
                (!tud_cdc_write_available() && time_us_64() > last_avail_time + 500000))
            {
                break;
            }
        }
    }
}

int stdio_usb_in_chars2(char *buf, int length)
{
    LockGuard lockGuard(*stdioMutex);
    if (!lockGuard.isLocked())
    {
        return PICO_ERROR_NO_DATA; // would deadlock otherwise
    }

    int rc = PICO_ERROR_NO_DATA;
    if (tud_cdc_connected() && tud_cdc_available())
    {
        int count = (int) tud_cdc_read(buf, (uint32_t) length);
        rc =  count ? count : PICO_ERROR_NO_DATA;
    }
    return rc;
}


struct stdio_driver stdio_usb2 =
{
    .out_chars = stdio_usb_out_chars2,
    .in_chars = stdio_usb_in_chars2,
#if PICO_STDIO_ENABLE_CRLF_SUPPORT
    // Replaces LF with CRLF
    .crlf_enabled = true
#endif
};

} // extern "C"

void cdc_init(MutexInterface* cdcStdioMutex)
{
    stdioMutex = cdcStdioMutex;
    stdio_set_driver_enabled(&stdio_usb2, true);
}

void cdc_task()
{
    if (!is_usb_cdc_en())
    {
        return;
    }

    // connected and there are data available
    if (tud_cdc_available())
    {
        if (ttyParser)
        {
            char buf[64];
            uint32_t count = 0;

            // read data (no need to lock this - this is the only place where read is done)
            count = tud_cdc_read(buf, sizeof(buf));

            if (count > 0)
            {
                if (echoOn)
                {
                    // Echo back (no crlf processing since calling directly)
                    stdio_usb_out_chars2(buf, count);
                }
                // Add to parser
                ttyParser->addChars(buf, count);
            }
        }
        else
        {
            // Parser not created yet
            tud_cdc_read_flush();
        }
    }
}

// Invoked when cdc when line state changed e.g connected/disconnected
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
  (void) itf;
  (void) rts;
}

// Invoked when CDC interface received data from host
void tud_cdc_rx_cb(uint8_t itf)
{
  (void) itf;
}

#endif // #if CFG_TUD_CDC
