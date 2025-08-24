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
#include "hal/Usb/TtyCommandHandler.hpp"
#include "hal/System/MutexInterface.hpp"

//! Command parser for processing commands from a TTY stream
class TtyParser
{
public:
    //! Virtual destructor
    virtual ~TtyParser() {}
    //! Adds a command parser to my list of parsers - must be done before any other function called
    virtual void addTtyCommandHandler(std::shared_ptr<TtyCommandHandler> parser) = 0;
    //! Called from the process receiving characters on the TTY
    virtual void addChars(const char* chars, uint32_t len) = 0;
    //! Called from the process handling maple bus execution
    virtual void process() = 0;
    //! When this character is seen, then binary data will proceed
    //! For binary commands, 2-byte size followed by payload then final \n character
    static const char BINARY_START_CHAR = TtyCommandHandler::BINARY_START_CHAR;
};

void usb_cdc_set_parser(TtyParser* parser);
