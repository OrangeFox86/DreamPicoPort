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

#ifndef __CONFIGURATION_H__
#define __CONFIGURATION_H__

// true to setup and print debug messages over UART0 (pin 1, 115200 8N1)
// Warning: enabling debug messages drastically degrades communication performance
#define SHOW_DEBUG_MESSAGES false

// true to enable USB CDC (serial) interface to directly control the maple bus
#define USB_CDC_ENABLED true

// Adjust the CPU clock frequency here (133 MHz is maximum documented stable frequency)
#define CPU_FREQ_KHZ 133000

// The minimum amount of time we check for an open line before taking control of it
// Set to 0 to completely disable this check
#define MAPLE_OPEN_LINE_CHECK_TIME_US 10

// Amount of time in nanoseconds at which each bit transmits (value should be divisible by 3)
// 480 ns achieves just over 2 mbps, just as the Dreamcast does
#define MAPLE_NS_PER_BIT 480

// Added percentage on top of the expected write completion duration to use for timeout
#define MAPLE_WRITE_TIMEOUT_EXTRA_PERCENT 20

// Estimated nanoseconds before peripheral responds - this is used for scheduling only
#define MAPLE_RESPONSE_DELAY_NS 50

// Maximum amount of time waiting for the beginning of a response when one is expected
#define MAPLE_RESPONSE_TIMEOUT_US 1000

// Estimated nanoseconds per bit to receive data - this is used for scheduling only
// 1750 was selected based on the average time it takes a Dreamcast controller to transmit each bit
#define MAPLE_RESPONSE_NS_PER_BIT 1750

// Maximum amount of time in microseconds to pass in between received words before read is canceled
// Dreamcast controllers sometimes have a ~180 us gap between words, so 300 us accommodates for that
#define MAPLE_INTER_WORD_READ_TIMEOUT_US 300

// The pin which sets IO direction for each player (-1 to disable)
#ifndef P1_DIR_PIN
    #define P1_DIR_PIN 6
#endif
#ifndef P2_DIR_PIN
    #define P2_DIR_PIN 7
#endif
#ifndef P3_DIR_PIN
    #define P3_DIR_PIN 26
#endif
#ifndef P4_DIR_PIN
    #define P4_DIR_PIN 27
#endif

// True if DIR pin is HIGH for output and LOW for input; false if opposite
#ifndef DIR_OUT_HIGH
    #define DIR_OUT_HIGH true
#endif

// The start pin of the two-pin bus for each player
#ifndef P1_BUS_START_PIN
    #define P1_BUS_START_PIN 10
#endif
#ifndef P2_BUS_START_PIN
    #define P2_BUS_START_PIN 12
#endif
#ifndef P3_BUS_START_PIN
    #define P3_BUS_START_PIN 18
#endif
#ifndef P4_BUS_START_PIN
    #define P4_BUS_START_PIN 20
#endif

// LED pin number for USB activity or -1 to disable
// When USB connected:
//   Default: ON
//   When controller key pressed: OFF
//   When controller disconnecting: Flashing slow
// When USB disconnected:
//   Default: OFF
//   When controller key pressed: Flashing quick
#ifndef USB_LED_PIN
    #define USB_LED_PIN 25
#endif

// LED pin number for simple USB activity or -1 to disable
// ON when USB connected; OFF when disconnected
#ifndef SIMPLE_USB_LED_PIN
    #define SIMPLE_USB_LED_PIN -1
#endif

#endif // __CONFIGURATION_H__
