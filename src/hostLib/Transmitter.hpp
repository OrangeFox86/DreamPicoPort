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
#include <memory>

struct Transmission;
struct MaplePacket;

class Transmitter
{
public:
    Transmitter() {}
    virtual ~Transmitter() {}

    //! Called when transmission has started to be sent
    //! @param[in] tx  The transmission that was sent
    virtual void txStarted(std::shared_ptr<const Transmission> tx) = 0;

    //! Called when transmission failed
    //! @param[in] writeFailed  Set to true iff TX failed because write failed
    //! @param[in] readFailed  Set to true iff TX failed because read failed
    //! @param[in] tx  The transmission that failed
    virtual void txFailed(bool writeFailed,
                          bool readFailed,
                          std::shared_ptr<Transmission> tx) = 0;

    //! Called when a transmission is complete
    //! @param[in] packet  The packet received or nullptr if this was write only transmission
    //! @param[in] tx  The transmission that triggered this data
    virtual void txComplete(std::shared_ptr<MaplePacket> packet,
                            std::shared_ptr<Transmission> tx) = 0;
};
