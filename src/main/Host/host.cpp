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

static volatile DppSettings::PlayerDetectionMode allMapleDetectionModes[MAX_DEVICES] = {
    DppSettings::PlayerDetectionMode::kDisable,
    DppSettings::PlayerDetectionMode::kDisable,
    DppSettings::PlayerDetectionMode::kDisable,
    DppSettings::PlayerDetectionMode::kDisable
};

static uint8_t mapleEnabledMask = 0;
static uint8_t numDevices = 0;
static volatile bool anyMapleAutoDetect = false;
static volatile bool allMapleAutoDetect = true;
static volatile uint8_t mapleIndexToPlayerIndex[MAX_DEVICES] = {};
static volatile DppSettings::PlayerDetectionMode mapleDetectionModes[MAX_DEVICES] = {};
static volatile bool mapleAutoDetectOnly[MAX_DEVICES] = {};
static std::vector<std::shared_ptr<DreamcastMainNode>> dreamcastMainNodes;
// Time markers for auto detect when anyMapleAutoDetect is true
static volatile uint64_t autoDetectTimeUs[MAX_DEVICES] = {};
static volatile uint64_t autoDetectReactionTimeUs = 0;

static DppSettings currentDppSettings;
static volatile bool core1Initialized = false;

std::unique_ptr<SerialStreamParser> make_tty_parser(
    const std::vector<std::shared_ptr<PrioritizedTxScheduler>>& schedulers,
    const std::vector<uint8_t>& mapleHostAddresses,
    const std::vector<std::shared_ptr<PlayerData>>& playerData
)
{
    // Initialize CDC to Maple Bus interfaces
    static Mutex ttyParserMutex;
    std::unique_ptr<SerialStreamParser> ttyParser = std::make_unique<SerialStreamParser>(ttyParserMutex, 'h');
    usb_cdc_set_parser(ttyParser.get());
    ttyParser->addCommandParser(
        std::make_shared<MaplePassthroughCommandParser>(
            schedulers, mapleHostAddresses
        )
    );
    static PicoIdentification picoIdentification;
    static Mutex flycastCommandParserMutex;
    ttyParser->addCommandParser(
        std::make_shared<FlycastCommandParser>(
            flycastCommandParserMutex,
            picoIdentification,
            schedulers,
            mapleHostAddresses,
            playerData,
            dreamcastMainNodes
        )
    );

    return ttyParser;
}

void setup_webusb_parsers(
    const std::vector<std::shared_ptr<PrioritizedTxScheduler>>& schedulers,
    const std::vector<uint8_t>& mapleHostAddresses,
    const std::vector<std::shared_ptr<PlayerData>>& playerData
)
{
    // Initialize WebUsb parsers
    std::shared_ptr<MapleWebUsbParser> mapleWebUsbParser =
        std::make_shared<MapleWebUsbParser>(
            schedulers,
            mapleHostAddresses
        );
    static PicoIdentification picoIdentification;
    std::shared_ptr<FlycastWebUsbParser> flycastWebUsbCommandParser =
        std::make_shared<FlycastWebUsbParser>(
            picoIdentification,
            mapleWebUsbParser,
            playerData,
            dreamcastMainNodes
        );
    webusb_add_parser(mapleWebUsbParser);
    webusb_add_parser(flycastWebUsbCommandParser);
}

// Second Core Process
// The second core is in charge of handling communication with Dreamcast peripherals
void core1()
{
    set_sys_clock_khz(CPU_FREQ_KHZ, true);

    // Wait for steady state
    sleep_ms(100);

    std::vector<uint8_t> mapleHostAddresses(MAX_DEVICES);
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
            else
            {
                allMapleAutoDetect = false;
            }
        }
    }
    mapleHostAddresses.resize(numDevices);

    CriticalSectionMutex screenMutexes[numDevices];
    std::shared_ptr<ScreenData> screenData[numDevices];
    std::vector<std::shared_ptr<PlayerData>> playerData;
    playerData.resize(numDevices);
    DreamcastControllerObserver** observers = get_usb_controller_observers();
    std::shared_ptr<MapleBusInterface> buses[numDevices];
    dreamcastMainNodes.resize(numDevices);
    Mutex schedulerMutexes[numDevices];
    std::vector<std::shared_ptr<PrioritizedTxScheduler>> schedulers(numDevices);
    Clock clock;
    uint8_t instanceId = 0;
    for (uint8_t i = 0; i < numDevices; ++i)
    {
        DreamcastControllerObserver& thisObserver = *(observers[mapleIndexToPlayerIndex[i]]);
        if (!mapleAutoDetectOnly[i])
        {
            thisObserver.setInstanceId(instanceId++);
        }

        screenData[i] = std::make_shared<ScreenData>(screenMutexes[i], i);
        playerData[i] = std::make_shared<PlayerData>(
            mapleIndexToPlayerIndex[i],
            thisObserver,
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

    std::unique_ptr<SerialStreamParser> ttyParser = make_tty_parser(schedulers, mapleHostAddresses, playerData);
    setup_webusb_parsers(schedulers, mapleHostAddresses, playerData);

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

void maple_detect(bool rebootNowOnDetect = false)
{
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
                    mapleEnabledMask |= (1 << playerIdx);
                    autoDetectTimeUs[i] = time_us_64();
                    autoDetectReactionTimeUs = (autoDetectTimeUs[i] + 500000);

                    if (allMapleDetectionModes[i] == DppSettings::PlayerDetectionMode::kAutoStatic)
                    {
                        // Update settings which will be saved later
                        currentDppSettings.playerDetectionModes[playerIdx] = DppSettings::PlayerDetectionMode::kEnable;
                    }
                }
            }
            else if (allMapleDetectionModes[i] != DppSettings::PlayerDetectionMode::kAutoDynamicNoDisable)
            {
                // Was connected, react on disconnect
                if (!dreamcastMainNodes[i]->isDeviceDetected())
                {
                    mapleEnabledMask &= ~(1 << playerIdx);
                    autoDetectTimeUs[i] = time_us_64();
                    autoDetectReactionTimeUs = (autoDetectTimeUs[i] + 500000);
                }
            }
        }
    }

    if (autoDetectReactionTimeUs > 0 && (rebootNowOnDetect || time_us_64() >= autoDetectReactionTimeUs))
    {
        usb_stop();

        watchdog_hw->scratch[0] = WATCHDOG_MAPLE_AUTO_DETECT_MAGIC;
        watchdog_hw->scratch[1] = mapleEnabledMask;

        if (DppSettings::getInitialSettings() != currentDppSettings)
        {
            // This should cause a reboot
            currentDppSettings.save();
        }

        watchdog_reboot(0, 0, 0);
    }
}

// First Core Process
// The first core is in charge of initialization and USB communication
int main()
{
    bool rebootDetected = (watchdog_hw->scratch[0] == WATCHDOG_MAPLE_AUTO_DETECT_MAGIC);

    set_sys_clock_khz(CPU_FREQ_KHZ, true);

    // Initialize settings from flash
    // This needs to be done before interrupts are enabled
    currentDppSettings = DppSettings::initialize();

    set_usb_cdc_en(currentDppSettings.cdcEn);
    set_usb_msc_en(currentDppSettings.mscEn);

    if (rebootDetected)
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

    Mutex fileMutex;
    Mutex cdcStdioMutex;
    Mutex webusbMutex;
    usb_init(&fileMutex, &cdcStdioMutex, &webusbMutex);

    multicore_launch_core1(core1);

    // Wait until core1 is done initializing
    while (!core1Initialized);

    if (allMapleAutoDetect && !rebootDetected)
    {
        // Run for 3.5 seconds to see if anything is initially detected (older VMUs may have 3 second beep)
        bool somethingDetected = false;
        uint64_t endTime = time_us_64() + 3500000;
        while (time_us_64() < endTime && !somethingDetected)
        {
            for (auto& node : dreamcastMainNodes)
            {
                if (node->isDeviceDetected())
                {
                    somethingDetected = true;
                }
            }
        }

        maple_detect(true);
    }

    static const uint64_t kMapleDetectPeriodUs = 125000;
    uint64_t nextMapleDetect = time_us_64() + kMapleDetectPeriodUs;

    usb_start();

    while(true)
    {
        usb_task();

        if (anyMapleAutoDetect && time_us_64() >= nextMapleDetect)
        {
            maple_detect();

            nextMapleDetect = time_us_64() + kMapleDetectPeriodUs;
        }
    }
}

#endif
