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

#include <cstdint>
#include <memory>
#include <vector>
#include <map>

#include "DreamcastNodeData.hpp"
#include "SerialStreamParser.hpp"

#include "hal/System/DppSettings.hpp"

//! Called periodically to signal aliveness (for watchdog reset)
void heartbeat();

//! Initializes the hardware (must be called from core0)
//! @param[in] core1Entry The core1 function entrypoint
//! @param[out] dcNodes The set of DreamcastNodeData [index] -> data
//! @param[out] runtimeAutoDetect Set to true if auto detection must be checked during runtime
void dpp_hw_init(void (*core1Entry)(), std::map<uint8_t, DreamcastNodeData>& dcNodes, bool& runtimeAutoDetect);

//! Creates all WebUSB and TTY parsers
//! @param[in] dcNodes All initialized DreamcastNodeData
//! @return The created TTY SerialStreamParser
std::unique_ptr<SerialStreamParser> make_parsers(const std::map<uint8_t, DreamcastNodeData>& dcNodes);

//! Performs maple bus detect/reboot logic
//! @param[in] dcNodes All initialized DreamcastNodeData
//! @param[in] rebootNowOnDetect When true, reboot occurs immediately on detection; false to apply delay
void maple_detect(const std::map<uint8_t, DreamcastNodeData>& dcNodes, bool rebootNowOnDetect = false);
