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

#ifndef ENABLE_UNIT_TEST

#include <memory>
#include <algorithm>

#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/exception.h"
#include "pico/multicore.h"
#include <hardware/watchdog.h>
#include <atomic>
#include <string.h>

#include "CriticalSectionMutex.hpp"
#include "Mutex.hpp"

#include "hal/Usb/usb_interface.hpp"
#include "hal/Usb/client_usb_interface.h"
#include "hal/Usb/WebUsbCommandHandler.hpp"

#include "host_setup.hpp"

const uint8_t MAPLE_HOST_ADDRESSES[MAX_DEVICES] = {0x00, 0x40, 0x80, 0xC0};
const uint32_t MAPLE_PINS[MAX_DEVICES] = {P1_BUS_START_PIN, P2_BUS_START_PIN, P3_BUS_START_PIN, P4_BUS_START_PIN};
const uint32_t MAPLE_DIR_PINS[MAX_DEVICES] = {P1_DIR_PIN, P2_DIR_PIN, P3_DIR_PIN, P4_DIR_PIN};

static std::map<uint8_t, DreamcastNodeData> dcNodes;

// Updated whenever core0 does a loop for watchdog check in core1
static std::atomic<bool> core0Alive = true;

// Exception handler for RP2040
void __not_in_flash_func(exception_handler)()
{
    while (true)
    {
        watchdog_reboot(0, 0, 0);
        busy_wait_ms(10);
    }
}

// Initialize and enable the hardware watchdog for shared use between cores.
static void heartbeat_setup()
{
    static constexpr uint32_t kSharedWatchdogTimeoutMs = 1000; // 1 second
    watchdog_enable(kSharedWatchdogTimeoutMs, true);
}

// Called from core 0 to signal heartbeat
static void core0_heartbeat()
{
    core0Alive.store(true, std::memory_order_relaxed);
}

// Called from core 1 to signal heartbeat
static void core1_heartbeat()
{
    // check value and reset to false in one operation
    // (relaxed because it doesn't need to synchronize with other data)
    if (core0Alive.exchange(false, std::memory_order_relaxed))
    {
        // We're both alive
        watchdog_update();
    }
}

// Second Core Process
// The second core is in charge of handling communication with Dreamcast peripherals
void core1()
{
#ifndef _DEBUG
    exception_set_exclusive_handler(HARDFAULT_EXCEPTION, exception_handler);
#endif

    // Initialize TTY parsers
    std::unique_ptr<SerialStreamParser> ttyParser = make_parsers(dcNodes);

    while(true)
    {
        // Process each main node
        for (auto& node : dcNodes)
        {
            // Worst execution duration of below is ~350 us at 133 MHz when debug print is disabled
            node.second.mainNode->task(time_us_64());
        }

        // Process any waiting commands in the TTY parser
        ttyParser->process();

        // Process any waiting commands in the WebUSB parser
        webusb_process();

        // Signal core 1 liveness to shared watchdog
        core1_heartbeat();
    }
}

// First Core Process
// The first core is in charge of initialization and USB communication
int main()
{
    set_sys_clock_khz(CPU_FREQ_KHZ, true);

#ifndef _DEBUG
    exception_set_exclusive_handler(HARDFAULT_EXCEPTION, exception_handler);
#endif

    const bool mapleRebootDetected = (watchdog_hw->scratch[0] == WATCHDOG_MAPLE_AUTO_DETECT_MAGIC);
    const bool settingsRebootDetected = (watchdog_hw->scratch[0] == DppSettings::WATCHDOG_SETTINGS_UPDATED_MAGIC);
    const bool usbCommandRebootDetected = (watchdog_hw->scratch[0] == WATCHDOG_SETTINGS_USB_REBOOT);
    const bool rebootDetected = (mapleRebootDetected || settingsRebootDetected || usbCommandRebootDetected);

    // Ensure USB hardware is not active
    usb_stop();

    // Wait for steady state
    sleep_ms(100);

    // Initialize settings from flash
    // This needs to be done before interrupts are enabled
    DppSettings currentDppSettings = DppSettings::initialize();
    currentDppSettings.makeValid(true);

    set_usb_cdc_en(currentDppSettings.cdcEn);
    set_usb_msc_en(currentDppSettings.mscEn);
    usb_webusb_link_announce_enable(currentDppSettings.webUsbAnnounceEn);

    if (mapleRebootDetected)
    {
        // Reboot occurred because auto maple detect changed states
        int32_t mask = 1;
        uint8_t i = 0;
        while (i < MAX_DEVICES)
        {
            if (
                currentDppSettings.playerDetectionModes[i] == DppSettings::PlayerDetectionMode::kEnable ||
                (mask & watchdog_hw->scratch[1]) != 0
            )
            {
                set_usb_descriptor_gamepad_en(i, true);
            }

            mask <<= 1;
            ++i;
        }
    }
    else if (settingsRebootDetected || usbCommandRebootDetected)
    {
        // Reboot occurred because settings updated or user commanded reboot
        int32_t mask = 1 << 8;
        uint8_t i = 0;
        while (i < MAX_DEVICES)
        {
            if (
                currentDppSettings.playerDetectionModes[i] == DppSettings::PlayerDetectionMode::kEnable ||
                (
                    currentDppSettings.playerDetectionModes[i] != DppSettings::PlayerDetectionMode::kDisable &&
                    (mask & watchdog_hw->scratch[1]) != 0
                )
            )
            {
                set_usb_descriptor_gamepad_en(i, true);
            }

            mask <<= 1;
            ++i;
        }
    }
    else
    {
        for (uint8_t i = 0; i < DppSettings::kNumPlayers; ++i)
        {
            if (currentDppSettings.playerDetectionModes[i] == DppSettings::PlayerDetectionMode::kEnable)
            {
                set_usb_descriptor_gamepad_en(i, true);
            }
        }
    }

    // These values are no longer needed
    watchdog_hw->scratch[0] = 0;
    watchdog_hw->scratch[1] = 0;

    std::vector<PlayerDefinition> playerDefs;
    playerDefs.reserve(MAX_DEVICES);
    bool anyMapleAutoDetect = false;
    bool allMapleAutoDetect = true;

    for (uint8_t i = 0; i < MAX_DEVICES; ++i)
    {
        bool usbEnabled = is_usb_descriptor_gamepad_en(i);
        bool autoDetect = (currentDppSettings.playerDetectionModes[i] > DppSettings::PlayerDetectionMode::kAutoThreshold);

        if (usbEnabled || autoDetect)
        {
            PlayerDefinition playerDef;

            playerDef.index = i;
            playerDef.mapleHostAddr = MAPLE_HOST_ADDRESSES[i];
            playerDef.gpioA = currentDppSettings.gpioA[i];
            playerDef.gpioDir =  currentDppSettings.gpioDir[i];
            playerDef.dirOutHigh = currentDppSettings.gpioDirOutputHigh[i];
            playerDef.detectionMode = currentDppSettings.playerDetectionModes[i];
            playerDef.autoDetectOnly = !usbEnabled;

            if (autoDetect)
            {
                anyMapleAutoDetect = true;
            }
            else
            {
                allMapleAutoDetect = false;
            }

            playerDefs.push_back(std::move(playerDef));
        }
    }

    // Convert DppSettings DpadType to DreamcastControllerObserver DpadType
    DreamcastControllerObserver::DpadType dpadType = DreamcastControllerObserver::DpadType::HAT;
    switch(currentDppSettings.dpadType)
    {
        case DppSettings::DpadType::kButtons:
            dpadType = DreamcastControllerObserver::DpadType::BUTTONS;
            break;

        case DppSettings::DpadType::kBoth:
            dpadType = DreamcastControllerObserver::DpadType::BOTH;
            break;

        case DppSettings::DpadType::kHat:
        default:
            dpadType = DreamcastControllerObserver::DpadType::HAT;
            break;
    }

    set_controller_dpad_type(dpadType);

    dcNodes = setup_dreamcast_nodes(playerDefs);

    maple_detect_init(dcNodes);

#if SHOW_DEBUG_MESSAGES
    stdio_uart_init();
#endif

    Mutex fileMutex;
    Mutex cdcStdioMutex;
    Mutex webusbMutex;
    usb_init(
        &fileMutex,
        &cdcStdioMutex,
        &webusbMutex,
        currentDppSettings.usbLedGpio,
        currentDppSettings.simpleUsbLedGpio
    );

    // Enable heartbeat watchdog
    heartbeat_setup();

    multicore_launch_core1(core1);

    if (allMapleAutoDetect && !rebootDetected && !dcNodes.empty())
    {
        // Run for 3.5 seconds to see if anything is initially detected (older VMUs may have 3 second beep)
        bool somethingDetected = false;
        uint64_t endTime = time_us_64() + 3500000;
        while (time_us_64() < endTime && !somethingDetected)
        {
            for (const std::pair<const uint8_t, DreamcastNodeData>& dcNode : dcNodes)
            {
                if (dcNode.second.mainNode->isDeviceDetected())
                {
                    somethingDetected = true;
                }
            }

            // Signal core 0 liveness to shared watchdog
            core0_heartbeat();
        }

        maple_detect(dcNodes, true);
    }

    static const uint32_t kMapleDetectPeriodUs = 125000;
    uint32_t lastMapleDetectTime = time_us_32();

    usb_start();

    // Callback to pass to DppSettings::processSaveRequests, to be called before save operations
    // This ensures the USB interface is properly stopped before reboot occurs
    std::function<void()> hwStopFn = usb_stop;

    while(true)
    {
        // Process USB operations (must be done on core 0)
        usb_task();

        // Process pending controller output (must be done on core 0)
        for (auto& node : dcNodes)
        {
            auto& pd = node.second.playerData;
            if (!pd) continue;
            pd->gamepad.process();
        }

        // Flush any outgoing WebUSB packets queued by other core (must be done on core 0)
        webusb_flush_outgoing();

        // Do any automatic detection of controllers (must be done on core 0)
        if (anyMapleAutoDetect && (time_us_32() - lastMapleDetectTime) >= kMapleDetectPeriodUs)
        {
            maple_detect(dcNodes);

            lastMapleDetectTime = time_us_32();
        }

        // Process save requests (must be done on core 0)
        DppSettings::processSaveRequests(hwStopFn);

        // Signal core 0 liveness to shared watchdog
        core0_heartbeat();
    }
}

#endif
