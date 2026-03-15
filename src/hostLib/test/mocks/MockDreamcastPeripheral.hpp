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

#include "DreamcastPeripheral.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

class MockDreamcastPeripheral : public DreamcastPeripheral
{
    public:
        MockDreamcastPeripheral(uint8_t addr,
                                uint32_t fd,
                                std::shared_ptr<EndpointTxSchedulerInterface> scheduler,
                                uint32_t playerIndex) :
            DreamcastPeripheral("mock", addr, fd, scheduler, playerIndex)
        {}

        MOCK_METHOD(void, txStarted, (std::shared_ptr<const Transmission> tx), (override));

        MOCK_METHOD(void,
                    txFailed,
                    (bool writeFailed,
                          bool readFailed,
                          std::shared_ptr<const Transmission> tx),
                    (override));

        MOCK_METHOD(void,
                    txComplete,
                    (std::shared_ptr<const MaplePacket> packet,
                        std::shared_ptr<const Transmission> tx),
                    (override));

        MOCK_METHOD(void, task, (uint64_t currentTimeUs), (override));

        MOCK_METHOD(uint32_t, getFunctionCode, (), (override));
};
