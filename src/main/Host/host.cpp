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

#define MAX_DEVICES 4

const uint8_t MAPLE_HOST_ADDRESSES[MAX_DEVICES] = {0x00, 0x40, 0x80, 0xC0};
const uint32_t MAPLE_PINS[MAX_DEVICES] = {P1_BUS_START_PIN, P2_BUS_START_PIN, P3_BUS_START_PIN, P4_BUS_START_PIN};
const uint32_t MAPLE_DIR_PINS[MAX_DEVICES] = {P1_DIR_PIN, P2_DIR_PIN, P3_DIR_PIN, P4_DIR_PIN};

static bool core1Initialized = false;

// Second Core Process
// The second core is in charge of handling communication with Dreamcast peripherals
void core1()
{
    set_sys_clock_khz(CPU_FREQ_KHZ, true);

    // Wait for steady state
    sleep_ms(100);

    uint32_t numDevices = 0;
    uint8_t mapleHostAddresses[MAX_DEVICES] = {};
    uint32_t maplePins[MAX_DEVICES] = {};
    int32_t mapleDirPins[MAX_DEVICES] = {};
    for (uint8_t i = 0; i < MAX_DEVICES; ++i)
    {
        if (is_usb_descriptor_gamepad_en(i))
        {
            mapleHostAddresses[numDevices] = MAPLE_HOST_ADDRESSES[i];
            maplePins[numDevices] = MAPLE_PINS[i];
            mapleDirPins[numDevices] = MAPLE_DIR_PINS[i];
            ++numDevices;
        }
    }

    CriticalSectionMutex screenMutexes[numDevices];
    std::shared_ptr<ScreenData> screenData[numDevices];
    std::vector<std::shared_ptr<PlayerData>> playerData;
    playerData.resize(numDevices);
    DreamcastControllerObserver** observers = get_usb_controller_observers();
    std::shared_ptr<MapleBusInterface> buses[numDevices];
    std::vector<std::shared_ptr<DreamcastMainNode>> dreamcastMainNodes;
    dreamcastMainNodes.resize(numDevices);
    Mutex schedulerMutexes[numDevices];
    std::shared_ptr<PrioritizedTxScheduler> schedulers[numDevices];
    Clock clock;
    for (uint32_t i = 0; i < numDevices; ++i)
    {
        screenData[i] = std::make_shared<ScreenData>(screenMutexes[i], i);
        playerData[i] = std::make_shared<PlayerData>(i,
                                                     *(observers[i]),
                                                     *screenData[i],
                                                     clock,
                                                     usb_msc_get_file_system());
        buses[i] = create_maple_bus(maplePins[i], mapleDirPins[i], DIR_OUT_HIGH);
        schedulers[i] = std::make_shared<PrioritizedTxScheduler>(schedulerMutexes[i], mapleHostAddresses[i]);
        dreamcastMainNodes[i] = std::make_shared<DreamcastMainNode>(
            *buses[i],
            *playerData[i],
            schedulers[i]);
    }

    // Initialize CDC to Maple Bus interfaces
    Mutex ttyParserMutex;
    SerialStreamParser* ttyParser = new SerialStreamParser(ttyParserMutex, 'h');
    usb_cdc_set_parser(ttyParser);
    ttyParser->addCommandParser(
        std::make_shared<MaplePassthroughCommandParser>(
            &schedulers[0], mapleHostAddresses, numDevices));
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
            dreamcastMainNodes));

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
    DppSettings settings = DppSettings::initialize();

    set_usb_cdc_en(settings.cdcEn);
    set_usb_msc_en(settings.mscEn);
    for (uint8_t i = 0; i < SELECTED_NUMBER_OF_DEVICES; ++i)
    {
        set_usb_descriptor_gamepad_en(i, true);
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
    }
}

#endif
