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

#include <cstdint>
#include <memory>
#include <vector>

#include "DreamcastMainNode.hpp"
#include "SerialStreamParser.hpp"

#include "hal/System/DppSettings.hpp"

#define MAX_DEVICES (DppSettings::kNumPlayers)

const uint32_t WATCHDOG_MAPLE_AUTO_DETECT_MAGIC = 0xEA68D4;

struct PlayerDefinition
{
    uint8_t index; //!< Player index [0,3]
    bool autoDetectOnly; //!< True if only waiting for detection, no USB communication
    DppSettings::PlayerDetectionMode detectionMode;
    uint32_t gpioA; //!< GPIO number of maple A, maple B is always very next one
    uint32_t gpioDir; //!< GPIO number of direction output
    bool dirOutHigh; //!< true if output is high when currently outputting, false for opposite
    uint8_t mapleHostAddr; //!< The address of the host on the maple bus
};

struct DreamcastNodeData
{
    PlayerDefinition playerDef;
    std::shared_ptr<DreamcastMainNode> mainNode;
    std::shared_ptr<PrioritizedTxScheduler> scheduler;
    std::shared_ptr<PlayerData> playerData;
};

std::vector<DreamcastNodeData> setup_dreamcast_nodes(const std::vector<PlayerDefinition>& playerDefs);
std::unique_ptr<SerialStreamParser> make_parsers(const std::vector<DreamcastNodeData>& dcNodes);
void maple_detect(
    uint8_t& mapleEnabledMask,
    DppSettings& currentDppSettings,
    std::vector<DreamcastNodeData>& dcNodes,
    bool rebootNowOnDetect = false
);
