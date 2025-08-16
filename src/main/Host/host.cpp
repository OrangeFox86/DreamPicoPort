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

#ifndef ENABLE_UNIT_TEST

#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "pico/multicore.h"
#include <hardware/flash.h>
#include <hardware/watchdog.h>
#include "hardware/pio.h"

#include "configuration.h"

#include "DreamcastMainNode.hpp"
#include "PlayerData.hpp"
#include "MaplePassthroughCommandParser.hpp"
#include "FlycastCommandParser.hpp"

#include "MapleWebUsbParser.hpp"
#include "FlycastWebUsbParser.hpp"

#include "CriticalSectionMutex.hpp"
#include "Mutex.hpp"
#include "Clock.hpp"
#include "PicoIdentification.cpp"
#include "SerialStreamParser.hpp"

#include "hal/System/LockGuard.hpp"
#include "hal/MapleBus/MapleBusInterface.hpp"
#include "hal/Usb/usb_interface.hpp"
#include "hal/Usb/client_usb_interface.h"
#include "hal/Usb/TtyParser.hpp"
#include "hal/Usb/client_usb_interface.h"
#include "hal/System/DppSettings.hpp"

#include <memory>
#include <algorithm>

#define MAX_DEVICES (DppSettings::kNumPlayers)

const uint32_t WATCHDOG_MAPLE_AUTO_DETECT_MAGIC = 0xEA68D4;
const uint8_t MAPLE_HOST_ADDRESSES[MAX_DEVICES] = {0x00, 0x40, 0x80, 0xC0};
const uint32_t MAPLE_PINS[MAX_DEVICES] = {P1_BUS_START_PIN, P2_BUS_START_PIN, P3_BUS_START_PIN, P4_BUS_START_PIN};
const uint32_t MAPLE_DIR_PINS[MAX_DEVICES] = {P1_DIR_PIN, P2_DIR_PIN, P3_DIR_PIN, P4_DIR_PIN};

static DppSettings::PlayerDetectionMode allMapleDetectionModes[MAX_DEVICES] = {
    DppSettings::PlayerDetectionMode::kDisable,
    DppSettings::PlayerDetectionMode::kDisable,
    DppSettings::PlayerDetectionMode::kDisable,
    DppSettings::PlayerDetectionMode::kDisable
};

static uint8_t mapleEnabledMask = 0;
static uint32_t numDevices = 0;
static bool anyMapleAutoDetect = false;
static uint8_t mapleIndexToPlayerIndex[MAX_DEVICES] = {};
static DppSettings::PlayerDetectionMode mapleDetectionModes[MAX_DEVICES] = {};
static bool mapleAutoDetectOnly[MAX_DEVICES] = {};
static std::vector<std::shared_ptr<DreamcastMainNode>> dreamcastMainNodes;
// Time markers for auto detect when anyMapleAutoDetect is true
static uint64_t autoDetectTimeUs[MAX_DEVICES] = {};
static uint64_t autoDetectReactionTimeUs = 0;

static DppSettings currentDppSettings;
static bool core1Initialized = false;

// Second Core Process
// The second core is in charge of handling communication with Dreamcast peripherals
void core1()
{
    set_sys_clock_khz(CPU_FREQ_KHZ, true);

    // Wait for steady state
    sleep_ms(100);

    uint8_t mapleHostAddresses[MAX_DEVICES] = {};
    uint32_t maplePins[MAX_DEVICES] = {};
    int32_t mapleDirPins[MAX_DEVICES] = {};
    for (uint8_t i = 0; i < MAX_DEVICES; ++i)
    {
        bool usbEnabled = is_usb_descriptor_gamepad_en(i);
        bool autoDetect = (allMapleDetectionModes[i] > DppSettings::PlayerDetectionMode::kAutoThreshold);

        if (usbEnabled || autoDetect)
        {
            mapleIndexToPlayerIndex[numDevices] = i;
            mapleHostAddresses[numDevices] = MAPLE_HOST_ADDRESSES[i];
            maplePins[numDevices] = MAPLE_PINS[i];
            mapleDirPins[numDevices] = MAPLE_DIR_PINS[i];
            mapleDetectionModes[numDevices] = allMapleDetectionModes[i];
            mapleAutoDetectOnly[numDevices] = !usbEnabled;
            if (usbEnabled)
            {
                mapleEnabledMask |= (1 << i);
            }
            ++numDevices;

            if (autoDetect)
            {
                anyMapleAutoDetect = true;
            }
        }
    }

    CriticalSectionMutex screenMutexes[numDevices];
    std::shared_ptr<ScreenData> screenData[numDevices];
    std::vector<std::shared_ptr<PlayerData>> playerData;
    playerData.resize(numDevices);
    DreamcastControllerObserver** observers = get_usb_controller_observers();
    std::shared_ptr<MapleBusInterface> buses[numDevices];
    dreamcastMainNodes.resize(numDevices);
    Mutex schedulerMutexes[numDevices];
    std::shared_ptr<PrioritizedTxScheduler> schedulers[numDevices];
    Clock clock;
    for (uint32_t i = 0; i < numDevices; ++i)
    {
        screenData[i] = std::make_shared<ScreenData>(screenMutexes[i], i);
        playerData[i] = std::make_shared<PlayerData>(
            i,
            *(observers[i]),
            *screenData[i],
            clock,
            usb_msc_get_file_system()
        );
        buses[i] = create_maple_bus(maplePins[i], mapleDirPins[i], DIR_OUT_HIGH);
        schedulers[i] = std::make_shared<PrioritizedTxScheduler>(schedulerMutexes[i], mapleHostAddresses[i]);
        dreamcastMainNodes[i] = std::make_shared<DreamcastMainNode>(
            *buses[i],
            *playerData[i],
            schedulers[i],
            mapleAutoDetectOnly[i]
        );
    }

    // Initialize CDC to Maple Bus interfaces
    Mutex ttyParserMutex;
    SerialStreamParser* ttyParser = new SerialStreamParser(ttyParserMutex, 'h');
    usb_cdc_set_parser(ttyParser);
    ttyParser->addCommandParser(
        std::make_shared<MaplePassthroughCommandParser>(
            &schedulers[0], mapleHostAddresses, numDevices
        )
    );
    PicoIdentification picoIdentification;
    Mutex flycastCommandParserMutex;
    ttyParser->addCommandParser(
        std::make_shared<FlycastCommandParser>(
            flycastCommandParserMutex,
            picoIdentification,
            &schedulers[0],
            mapleHostAddresses,
            numDevices,
            playerData,
            dreamcastMainNodes
        )
    );

    // Initialize WebUsb parsers
    std::shared_ptr<MapleWebUsbParser> mapleWebUsbParser =
        std::make_shared<MapleWebUsbParser>(
            &schedulers[0],
            mapleHostAddresses,
            numDevices
        );
    std::shared_ptr<FlycastWebUsbParser> flycastWebUsbCommandParser =
        std::make_shared<FlycastWebUsbParser>(
            picoIdentification,
            mapleWebUsbParser,
            playerData,
            dreamcastMainNodes
        );
    webusb_add_parser(mapleWebUsbParser);
    webusb_add_parser(flycastWebUsbCommandParser);

    // Done initialized
    core1Initialized = true;

    while(true)
    {
        // Process each main node
        for (auto& node : dreamcastMainNodes)
        {
            // Worst execution duration of below is ~350 us at 133 MHz when debug print is disabled
            node->task(time_us_64());
        }

        // Process any waiting commands in the TTY parser
        ttyParser->process();
    }
}

// First Core Process
// The first core is in charge of initialization and USB communication
int main()
{
    set_sys_clock_khz(CPU_FREQ_KHZ, true);

    // Initialize settings from flash
    // This needs to be done before interrupts are enabled
    currentDppSettings = DppSettings::initialize();

    set_usb_cdc_en(currentDppSettings.cdcEn);
    set_usb_msc_en(currentDppSettings.mscEn);

    if (watchdog_hw->scratch[0] == WATCHDOG_MAPLE_AUTO_DETECT_MAGIC)
    {
        // Reboot occurred because auto detect changed states
        uint8_t mask = 1;
        uint8_t i = 0;
        while (i < MAX_DEVICES)
        {
            allMapleDetectionModes[i] = currentDppSettings.playerDetectionModes[i];

            if ((mask & watchdog_hw->scratch[1]) != 0)
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
            allMapleDetectionModes[i] = currentDppSettings.playerDetectionModes[i];

            if (currentDppSettings.playerDetectionModes[i] == DppSettings::PlayerDetectionMode::kEnable)
            {
                set_usb_descriptor_gamepad_en(i, true);
            }
        }
    }

#if SHOW_DEBUG_MESSAGES
    stdio_uart_init();
#endif

    multicore_launch_core1(core1);

    // Wait until core1 is done initializing
    while (!core1Initialized);

    Mutex fileMutex;
    Mutex cdcStdioMutex;
    Mutex webusbMutex;
    usb_init(&fileMutex, &cdcStdioMutex, &webusbMutex);

    while(true)
    {
        usb_task();

        if (anyMapleAutoDetect)
        {
            if (time_us_64() < 500000)
            {
                continue;
            }

            if (autoDetectReactionTimeUs > 0 && time_us_64() >= autoDetectReactionTimeUs)
            {
                printf("REACT\n");
                fflush(stdout);
                autoDetectReactionTimeUs = 0;

                watchdog_hw->scratch[0] = WATCHDOG_MAPLE_AUTO_DETECT_MAGIC;
                watchdog_hw->scratch[1] = mapleEnabledMask;

                if (DppSettings::getInitialSettings() != currentDppSettings)
                {
                    // This should cause a reboot
                    currentDppSettings.save();
                }

                watchdog_reboot(0, 0, 50);
            }

            for (uint32_t i = 0; i < numDevices; ++i)
            {
                DppSettings::PlayerDetectionMode mode = mapleDetectionModes[i];
                if (mode > DppSettings::PlayerDetectionMode::kAutoThreshold && autoDetectTimeUs[i] == 0)
                {
                    const uint8_t playerIdx = mapleIndexToPlayerIndex[i];
                    if (mapleAutoDetectOnly[i])
                    {
                        // Was disconnected, react on connection
                        if (dreamcastMainNodes[i]->isDeviceDetected())
                        {
                            printf("Device detected\n");

                            mapleEnabledMask |= (1 << playerIdx);
                            autoDetectTimeUs[i] = time_us_64();
                            autoDetectReactionTimeUs = (autoDetectTimeUs[i] + 500000);

                            if (allMapleDetectionModes[i] != DppSettings::PlayerDetectionMode::kAutoDynamic)
                            {
                                // Update settings which will be saved later
                                currentDppSettings.playerDetectionModes[playerIdx] = DppSettings::PlayerDetectionMode::kEnable;
                            }

                            printf("Enable mask: %02hx; auto detect time: %llu; reaction time: %llu; detect: ", mapleEnabledMask, autoDetectTimeUs[i], autoDetectReactionTimeUs);

                            for (uint8_t i = 0; i < DppSettings::kNumPlayers; ++i)
                            {
                                printf("%02hx ", static_cast<uint8_t>(currentDppSettings.playerDetectionModes[i]));
                            }

                            printf("\n");

                            fflush(stdout);
                        }
                    }
                    else
                    {
                        // Was connected, react on disconnect
                        if (!dreamcastMainNodes[i]->isDeviceDetected())
                        {
                            printf("Device no longer detected\n");
                            fflush(stdout);

                            mapleEnabledMask &= ~(1 << playerIdx);
                            autoDetectTimeUs[i] = time_us_64();
                            autoDetectReactionTimeUs = (autoDetectTimeUs[i] + 500000);
                        }
                    }
                }
            }
        }
    }
}

#endif
