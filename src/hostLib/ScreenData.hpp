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

#include "hal/System/MutexInterface.hpp"
#include <stdint.h>

//! Contains monochrome screen data
//! A screen is 48 bits wide and 32 bits tall
class ScreenData
{
    public:
        //! Constructor
        //! @param[in] mutex  Reference to the mutex to use (critical section mutex recommended)
        ScreenData(MutexInterface& mutex, uint32_t defaultScreenNum=0);

        //! Set the screen bits
        //! @param[in] data  Screen words to set
        //! @param[in] startIndex  Starting screen word index (left to right, top to bottom)
        //! @param[in] numWords  Number of words to write
        //! @param[in] update  True to mark the screen data as updated
        void setData(
            const uint32_t* data,
            uint32_t startIndex=0,
            uint32_t numWords=NUM_SCREEN_WORDS,
            bool update=true);

        //! Sets the current screen to one of the 4 defaults
        //! @param[in] defaultScreenNum  The default screen number to set the screen to
        void setDataToADefault(uint32_t defaultScreenNum);

        //! Resets the screen to its initialized default
        void resetToDefault();

        //! @returns true if new data is available since last call to readData
        bool isNewDataAvailable() const;

        //! Copies screen data to the given array
        //! @param[out] out  The array to write to (must be at least 48 words in length)
        void readData(uint32_t* out);

    public:
        //! Number of words in a screen
        static const uint8_t NUM_SCREEN_WORDS = 48;
        //! Number of default screens
        static const uint32_t NUM_DEFAULT_SCREENS = 4;

    private:
        //! The default screen data on initialization and resetToDefault()
        static const uint32_t DEFAULT_SCREENS[NUM_DEFAULT_SCREENS][NUM_SCREEN_WORDS];
        //! Mutex used to ensure integrity of data between multiple cores
        MutexInterface& mMutex;
        //! Default screen to revert to on resetToDefault()
        uint32_t mDefaultScreen[NUM_SCREEN_WORDS];
        //! The current screen data
        uint32_t mScreenData[NUM_SCREEN_WORDS];
        //! Flag set to true in setData and set to false in readData
        bool mNewDataAvailable;
};
