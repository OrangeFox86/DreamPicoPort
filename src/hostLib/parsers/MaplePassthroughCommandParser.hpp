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

#include "hal/Usb/CommandParser.hpp"

#include "PrioritizedTxScheduler.hpp"

#include <memory>

// Command structure: [whitespace]<command-char>[command]<\n>

//! Command parser for processing commands from a TTY stream
class MaplePassthroughCommandParser : public CommandParser
{
public:
    MaplePassthroughCommandParser(std::shared_ptr<PrioritizedTxScheduler>* schedulers,
                                  const uint8_t* senderAddresses,
                                  uint32_t numSenders);

    //! @returns the string of command characters this parser handles
    virtual const char* getCommandChars() final;

    //! Called when newline reached; submit command and reset
    virtual void submit(const char* chars, uint32_t len) final;

    //! Prints help message for this command
    virtual void printHelp() final;

private:
    std::shared_ptr<PrioritizedTxScheduler>* const mSchedulers;
    const uint8_t* const mSenderAddresses;
    const uint32_t mNumSenders;
};
