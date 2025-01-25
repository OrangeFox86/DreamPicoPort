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

#pragma once

#include <stdint.h>

//! Structure used to unpack an 8-byte controller condition package (little endian assumed)
typedef struct controller_condition_s
{
    uint8_t l; //!< 0: fully released; 255: fully pressed

    uint8_t r; //!< 0: fully released; 255: fully pressed

    // Digital bits:
    // 0: pressed
    // 1: released
    unsigned z:1;
    unsigned y:1;
    unsigned x:1;
    unsigned d:1;
    unsigned upb:1;
    unsigned downb:1;
    unsigned leftb:1;
    unsigned rightb:1;

    unsigned c:1;
    unsigned b:1;
    unsigned a:1;
    unsigned start:1;
    unsigned up:1;
    unsigned down:1;
    unsigned left:1;
    unsigned right:1;

    uint8_t rAnalogUD; //!< 0: up; 128: neutral; 255: down

    uint8_t rAnalogLR; //!< 0: up; 128: neutral; 255: down

    uint8_t lAnalogUD; //!< 0: up; 128: neutral; 255: down

    uint8_t lAnalogLR; //!< 0: left; 128: neutral; 255: right
}__attribute__((packed)) controller_condition_t;

#define NEUTRAL_CONTROLLER_CONDITION {0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 128, 128, 128, 128}

//! Button condition bits found in the VMU timer function (least significant byte)
typedef struct vmu_timer_condition_s
{
    // Digital bits:
    // 0: pressed
    // 1: released
    unsigned up:1;
    unsigned down:1;
    unsigned left:1;
    unsigned right:1;
    unsigned a:1;
    unsigned b:1;
    unsigned c:1;
    unsigned start:1;
} __attribute__((packed)) vmu_timer_condition_t;

#define NEUTRAL_VMU_TIMER_CONDITION {1, 1, 1, 1, 1, 1, 1, 1}
