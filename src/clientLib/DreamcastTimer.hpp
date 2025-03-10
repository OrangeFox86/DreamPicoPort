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

#include "DreamcastPeripheralFunction.hpp"
#include "dreamcast_constants.h"

#include <string.h>

#include "hal/System/ClockInterface.hpp"

namespace client
{
class DreamcastTimer : public DreamcastPeripheralFunction
{
public:
    struct DateTime
    {
        //! Calendar year ex 1999 for 1999
        uint16_t year;
        //! Calendar month ex 1 for January
        uint8_t month;
        //! Calendar day ex 31 for the 31st day of the month
        uint8_t day;
        //! Clock hour ex 0 for midnight
        uint8_t hour;
        //! Clock minute ex 0 for midnight
        uint8_t minute;
        //! Clock second ex 0 for midnight
        uint8_t second;
        //! Calendar day of week where 0 is monday
        uint8_t dayOfWeek;
    };

    struct SetTime
    {
        //! The clock time that correlates to the following date
        uint64_t baseTime;
        //! The date and time
        DateTime dateTime;
    };

    //! Callback function definition which is executed when host sets time of the timer
    //! @param[in] setTime  The time given by the host
    typedef void (*SetTimeFn)(const SetTime& setTime);

    //! Callback function definition which is executed when the host sets PWM data for audible
    //! alarm. Alarm is based on the frequency about 1000000 Hz (measured).
    //! @param[in] width  Number of counts until the waveform repeats
    //! @param[in] down  Number of counts signal is low (will be less than or equal to width)
    typedef void (*SetPwmFn)(uint8_t width, uint8_t down);

public:
    DreamcastTimer(const ClockInterface& clock, SetTimeFn setTimeFn, SetPwmFn setPwmFn);

    //! Handle packet meant for this peripheral function
    //! @param[in] in  The packet read from the Maple Bus
    //! @param[out] out  The packet to write to the Maple Bus when true is returned
    //! @returns true iff the packet was handled
    virtual bool handlePacket(const MaplePacket& in, MaplePacket& out) final;

    //! Called when player index changed or timeout occurred
    virtual void reset() final;

    //! @returns the function definition for this peripheral function
    virtual uint32_t getFunctionDefinition() final;

    DateTime getCurrentDateTime();

private:
    const ClockInterface& mClock;
    const SetTimeFn mSetTimeFn;
    const SetPwmFn mSetPwmFn;
    SetTime mSetTime;
};
}
