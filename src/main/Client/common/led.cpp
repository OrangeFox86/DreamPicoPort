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

#include "led.hpp"

#include "configuration.h"

#include "pico/stdlib.h"

// Only a single LED function for client
#define CLIENT_LED_PIN ((USB_LED_PIN >= 0) ? USB_LED_PIN : SIMPLE_USB_LED_PIN)

void led_init()
{
    if (CLIENT_LED_PIN >= 0)
    {
        gpio_init(CLIENT_LED_PIN);
        gpio_set_dir_out_masked(1<<CLIENT_LED_PIN);
    }
}

void led_task(uint64_t lastActivityTimeUs)
{
    static bool ledOn = false;
    static uint64_t startUs = 0;
    static const uint32_t BLINK_TIME_US = 250000;
    static const uint32_t ACTIVITY_DELAY_US = 500000;

    // To correct for the non-atomic read in getLastActivityTime(), only update activityStopTime
    // if a new time is greater
    uint64_t currentTime = time_us_64();
    static uint64_t activityStopTime = 0;
    if (lastActivityTimeUs != 0)
    {
        lastActivityTimeUs += ACTIVITY_DELAY_US;
        if (lastActivityTimeUs > activityStopTime)
        {
            activityStopTime = lastActivityTimeUs;
        }
    }

    if (activityStopTime > currentTime)
    {
        uint64_t t = currentTime - startUs;
        if (t >= BLINK_TIME_US)
        {
            startUs += BLINK_TIME_US;
            ledOn = !ledOn;
        }
    }
    else
    {
        startUs = currentTime;
        ledOn = true;
    }

    gpio_put(CLIENT_LED_PIN, ledOn);
}
