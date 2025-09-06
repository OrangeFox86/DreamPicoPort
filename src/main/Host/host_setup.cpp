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

#include "host_setup.hpp"

#include <hardware/watchdog.h>

#include "hal/Usb/usb_interface.hpp"
#include "hal/Usb/client_usb_interface.h"

#include "MaplePassthroughTtyCommandHandler.hpp"
#include "FlycastTtyCommandHandler.hpp"

#include "MapleWebUsbCommandHandler.hpp"
#include "FlycastWebUsbCommandHandler.hpp"
#include "SettingsWebUsbCommandHandler.hpp"

#include "PicoIdentification.hpp"
#include "CriticalSectionMutex.hpp"
#include "Mutex.hpp"
#include "Clock.hpp"

std::map<uint8_t, DreamcastNodeData> setup_dreamcast_nodes(const std::vector<PlayerDefinition>& playerDefs)
{
    std::map<uint8_t, DreamcastNodeData> dcNodeData;

    static CriticalSectionMutex screenMutexes[MAX_DEVICES];
    DreamcastControllerObserver** observers = get_usb_controller_observers();
    static Mutex schedulerMutexes[MAX_DEVICES];
    static Clock clock;
    uint8_t instanceId = 0;
    for (const PlayerDefinition& playerDef : playerDefs)
    {
        DreamcastControllerObserver& thisObserver = *(observers[playerDef.index]);
        if (!playerDef.autoDetectOnly)
        {
            thisObserver.setInstanceId(instanceId++);
        }

        DreamcastNodeData thisNode;

        thisNode.playerDef = std::make_shared<PlayerDefinition>(playerDef);
        thisNode.playerData = std::make_shared<PlayerData>(PlayerData{
            .playerIndex = playerDef.index,
            .gamepad = thisObserver,
            .screenData = std::make_shared<ScreenData>(screenMutexes[playerDef.index], playerDef.index),
            .clock = clock,
            .fileSystem = usb_msc_get_file_system()
        });
        thisNode.scheduler = std::make_shared<PrioritizedTxScheduler>(
            schedulerMutexes[playerDef.index],
            playerDef.mapleHostAddr
        );
        thisNode.mainNode = std::make_shared<DreamcastMainNode>(
            create_maple_bus(playerDef.gpioA, playerDef.gpioDir, playerDef.dirOutHigh),
            thisNode.playerData,
            thisNode.scheduler,
            playerDef.autoDetectOnly
        );

        dcNodeData.insert_or_assign(playerDef.index, std::move(thisNode));
    }

    return dcNodeData;
}

std::unique_ptr<SerialStreamParser> make_parsers(
    const std::map<uint8_t, DreamcastNodeData>& dcNodes
)
{
    // Initialize CDC to Maple Bus interfaces
    static Mutex ttyParserMutex;
    std::unique_ptr<SerialStreamParser> ttyParser = std::make_unique<SerialStreamParser>(ttyParserMutex, 'h');
    usb_cdc_set_parser(ttyParser.get());
    ttyParser->addTtyCommandHandler(std::make_shared<MaplePassthroughTtyCommandHandler>(dcNodes));
    static PicoIdentification picoIdentification;
    static Mutex flycastTtyCommandHandlerMutex;
    ttyParser->addTtyCommandHandler(
        std::make_shared<FlycastTtyCommandHandler>(
            flycastTtyCommandHandlerMutex,
            picoIdentification,
            dcNodes
        )
    );

    // Initialize and register WebUsb parsers
    std::shared_ptr<MapleWebUsbCommandHandler> mapleWebUsbCommandHandler =
        std::make_shared<MapleWebUsbCommandHandler>(dcNodes);
    webusb_add_parser(mapleWebUsbCommandHandler);
    std::shared_ptr<FlycastWebUsbCommandHandler> flycastWebUsbCommandParser =
        std::make_shared<FlycastWebUsbCommandHandler>(
            picoIdentification,
            mapleWebUsbCommandHandler,
            dcNodes
        );
    webusb_add_parser(flycastWebUsbCommandParser);
    std::shared_ptr<SettingsWebUsbCommandHandler> settingsWebUsbCommandHandler = std::make_shared<SettingsWebUsbCommandHandler>();
    webusb_add_parser(settingsWebUsbCommandHandler);

    return ttyParser;
}

static uint8_t mapleEnabledMask = 0;
static DppSettings::PlayerDetectionMode mapleDetectUpdatedModes[DppSettings::kNumPlayers];
static bool maplePlayerModesUpdated = false;

void maple_detect_init(const std::map<uint8_t, DreamcastNodeData>& dcNodes)
{
    for (uint8_t i = 0; i < DppSettings::kNumPlayers; ++i)
    {
        mapleDetectUpdatedModes[i] = DppSettings::getInitialSettings().playerDetectionModes[i];
    }

    for (const std::pair<const uint8_t, DreamcastNodeData>& node : dcNodes)
    {
        if (!node.second.playerDef->autoDetectOnly)
        {
            mapleEnabledMask |= (1 << node.first);
        }
    }
}

void maple_detect(std::map<uint8_t, DreamcastNodeData>& dcNodes, bool rebootNowOnDetect)
{
    // Time markers for auto detect when anyMapleAutoDetect is true
    static uint64_t autoDetectTimeUs[MAX_DEVICES] = {};
    static uint64_t autoDetectReactionTimeUs = 0;

    uint8_t i = 0;
    for (const std::pair<const uint8_t, DreamcastNodeData>& dcNode : dcNodes)
    {
        const DppSettings::PlayerDetectionMode& mode = dcNode.second.playerDef->detectionMode;

        if (mode > DppSettings::PlayerDetectionMode::kAutoThreshold && autoDetectTimeUs[i] == 0)
        {
            const uint8_t playerIdx = dcNode.first;
            if (dcNode.second.playerDef->autoDetectOnly)
            {
                // Was disconnected on boot, react on connection
                if (dcNode.second.mainNode->isDeviceDetected())
                {
                    mapleEnabledMask |= (1 << playerIdx);
                    autoDetectTimeUs[i] = time_us_64();
                    autoDetectReactionTimeUs = (autoDetectTimeUs[i] + 500000);

                    if (mode == DppSettings::PlayerDetectionMode::kAutoStatic)
                    {
                        // Update settings which will be saved later
                        mapleDetectUpdatedModes[playerIdx] = DppSettings::PlayerDetectionMode::kEnable;
                        maplePlayerModesUpdated = true;
                    }
                }
            }
            else if (mode == DppSettings::PlayerDetectionMode::kAutoDynamic)
            {
                // Was connected on boot, react on disconnect
                if (!dcNode.second.mainNode->isDeviceDetected())
                {
                    mapleEnabledMask &= ~(1 << playerIdx);
                    autoDetectTimeUs[i] = time_us_64();
                    autoDetectReactionTimeUs = (autoDetectTimeUs[i] + 500000);
                }
            }
        }

        ++i;
    }

    if (autoDetectReactionTimeUs > 0 && (rebootNowOnDetect || time_us_64() >= autoDetectReactionTimeUs))
    {
        usb_stop();

        watchdog_hw->scratch[0] = WATCHDOG_MAPLE_AUTO_DETECT_MAGIC;
        watchdog_hw->scratch[1] = mapleEnabledMask;

        if (maplePlayerModesUpdated)
        {
            DppSettings newSettings = DppSettings::getInitialSettings();
            for (uint8_t i = 0; i < DppSettings::kNumPlayers; ++i)
            {
                newSettings.playerDetectionModes[i] = mapleDetectUpdatedModes[i];
            }

            // This should cause a reboot
            newSettings.save();
        }

        watchdog_reboot(0, 0, 0);
    }
}
