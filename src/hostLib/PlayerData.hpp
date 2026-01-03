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

#include "hal/Usb/DreamcastControllerObserver.hpp"
#include "hal/System/ClockInterface.hpp"
#include "ScreenData.hpp"
#include "hal/Usb/UsbFileSystem.hpp"
#include <atomic>
#include <cstdint>
#include <cstring>

//! Contains data that is tied to a specific player
struct PlayerData
{
    const uint32_t playerIndex;
    DreamcastControllerObserver& gamepad;
    std::shared_ptr<ScreenData> screenData;
    ClockInterface& clock;
    UsbFileSystem& fileSystem;

    // Atomic snapshot fields for cross-core communication (core1 -> core0)
    std::atomic<uint64_t> latest_controller_condition{0};
    std::atomic<bool> controller_condition_updated{false};

    std::atomic<bool> pending_connected{false};
    std::atomic<bool> pending_disconnected{false};

    std::atomic<bool> pending_change{false};
    std::atomic<uint32_t> pending_change_value{0};
    PlayerData(
        uint32_t playerIndex_,
        DreamcastControllerObserver& gamepad_,
        std::shared_ptr<ScreenData> screenData_,
        ClockInterface& clock_,
        UsbFileSystem& fileSystem_
    ) :
        playerIndex(playerIndex_),
        gamepad(gamepad_),
        screenData(screenData_),
        clock(clock_),
        fileSystem(fileSystem_),
        latest_controller_condition(0),
        controller_condition_updated(false),
        pending_connected(false),
        pending_disconnected(false),
        pending_change(false),
        pending_change_value(0)
    {}
};

