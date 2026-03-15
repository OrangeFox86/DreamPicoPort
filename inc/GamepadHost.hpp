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

#pragma once

#include <stdint.h>

class GamepadHost
{
public:
    inline GamepadHost() {}
    inline virtual ~GamepadHost() {}

    //! Enumerates hat positions
    enum class Hat : uint8_t
    {
        NEUTRAL = 0,
        UP,
        UP_RIGHT,
        RIGHT,
        DOWN_RIGHT,
        DOWN,
        DOWN_LEFT,
        LEFT,
        UP_LEFT
    };

    //! Standard set of gamepad controls
    struct Controls
    {
        Hat hat;
        bool west;
        bool south;
        bool east;
        bool north;

        bool l1;
        bool r1;
        uint8_t l2;
        uint8_t r2;
        bool l3;
        bool r3;

        bool start;
        bool menu;

        uint8_t lx;
        uint8_t ly;
        uint8_t rx;
        uint8_t ry;
    };

    //! Set updated controls
    //! @param[in] controls  Updated controls
    virtual void setControls(const Controls& controls) = 0;
};

void set_gamepad_host(GamepadHost* ctrlr);
