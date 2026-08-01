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

#ifndef ENABLE_UNIT_TEST

#include <memory>

#include <pico/stdlib.h>
#include <hardware/exception.h>
#include <hardware/watchdog.h>

#include "hal/Usb/usb_interface.hpp"
#include "hal/Usb/WebUsbCommandHandler.hpp"

#include "host_setup.hpp"

static std::map<uint8_t, DreamcastNodeData> dcNodes;

// Exception handler for RP2040
void __not_in_flash_func(exception_handler)()
{
    while (true)
    {
        watchdog_reboot(0, 0, 0);
        busy_wait_ms(10);
    }
}

// Second Core Process
// The second core is in charge of handling communication with Dreamcast peripherals
void core1()
{
#ifndef _DEBUG
    exception_set_exclusive_handler(HARDFAULT_EXCEPTION, exception_handler);
#endif

    // Initialize TTY parsers
    const std::unique_ptr<SerialStreamParser> ttyParser = make_parsers(dcNodes);

    while(true)
    {
        // Process each main node
        for (auto& node : dcNodes)
        {
            // Worst execution duration of below is ~350 us at 133 MHz when debug print is disabled
            node.second.mainNode->task(time_us_64());
        }

        // Process any waiting commands in the TTY parser
        ttyParser->process();

        // Process any waiting commands in the WebUSB parser
        webusb_process();

        // Signal core 1 liveness to shared watchdog
        heartbeat();
    }
}

// First Core Process
// The first core is in charge of initialization and USB communication
int main()
{
#ifndef _DEBUG
    exception_set_exclusive_handler(HARDFAULT_EXCEPTION, exception_handler);
#endif

    bool runtimeAutoDetect = false;
    dpp_hw_init(core1, dcNodes, runtimeAutoDetect);

    static const uint32_t kMapleDetectPeriodUs = 125000;
    uint32_t lastMapleDetectTime = time_us_32();

    // Callback to pass to DppSettings::processSaveRequests, to be called before save operations
    // This ensures the USB interface is properly stopped before reboot occurs
    const std::function<void()> hwStopFn = usb_stop;

    while(true)
    {
        // Process USB operations (must be done on core 0)
        usb_task();

        // Process pending controller output (must be done on core 0)
        for (auto& node : dcNodes)
        {
            auto& pd = node.second.playerData;
            if (pd)
            {
                pd->gamepad.process();
            }
        }

        // Flush any outgoing WebUSB packets queued by other core (must be done on core 0)
        webusb_flush_outgoing();

        // Do any automatic detection of controllers (must be done on core 0)
        if (runtimeAutoDetect && (time_us_32() - lastMapleDetectTime) >= kMapleDetectPeriodUs)
        {
            maple_detect(dcNodes);

            lastMapleDetectTime = time_us_32();
        }

        // Process save requests (must be done on core 0)
        DppSettings::processSaveRequests(hwStopFn);

        // Signal core 0 liveness to shared watchdog
        heartbeat();
    }
}

#endif
