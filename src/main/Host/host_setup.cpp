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

#include "MaplePassthroughCommandParser.hpp"
#include "FlycastCommandParser.hpp"

#include "MapleWebUsbParser.hpp"
#include "FlycastWebUsbParser.hpp"

#include "FlycastCommandParser.hpp"
#include "PicoIdentification.hpp"
#include "CriticalSectionMutex.hpp"
#include "Mutex.hpp"
#include "Clock.hpp"

std::vector<DreamcastNodeData> setup_dreamcast_nodes(const std::vector<PlayerDefinition>& playerDefs)
{
    std::vector<DreamcastNodeData> dcNodeData;
    dcNodeData.reserve(playerDefs.size());

    static CriticalSectionMutex screenMutexes[MAX_DEVICES];
    DreamcastControllerObserver** observers = get_usb_controller_observers();
    static Mutex schedulerMutexes[MAX_DEVICES];
    static Clock clock;
    uint8_t instanceId = 0;
    uint8_t i = 0;
    for (const PlayerDefinition& playerDef : playerDefs)
    {
        DreamcastControllerObserver& thisObserver = *(observers[playerDef.index]);
        if (!playerDef.autoDetectOnly)
        {
            thisObserver.setInstanceId(instanceId++);
        }

        DreamcastNodeData thisNode;

        thisNode.playerDef = playerDef;
        thisNode.playerData = std::make_shared<PlayerData>(PlayerData{
            .playerIndex = playerDef.index,
            .gamepad = thisObserver,
            .screenData = std::make_shared<ScreenData>(screenMutexes[i], i),
            .clock = clock,
            .fileSystem = usb_msc_get_file_system()
        });
        thisNode.scheduler = std::make_shared<PrioritizedTxScheduler>(schedulerMutexes[i], playerDef.mapleHostAddr);
        thisNode.mainNode = std::make_shared<DreamcastMainNode>(
            create_maple_bus(playerDef.gpioA, playerDef.gpioDir, playerDef.dirOutHigh),
            thisNode.playerData,
            thisNode.scheduler,
            playerDef.autoDetectOnly
        );

        dcNodeData.push_back(std::move(thisNode));
        ++i;
    }

    return dcNodeData;
}

std::unique_ptr<SerialStreamParser> make_tty_parser(
    const std::vector<std::shared_ptr<PrioritizedTxScheduler>>& schedulers,
    const std::vector<uint8_t>& mapleHostAddresses,
    const std::vector<std::shared_ptr<PlayerData>>& playerData,
    const std::vector<std::shared_ptr<DreamcastMainNode>>& dreamcastMainNodes
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
    const std::vector<std::shared_ptr<PlayerData>>& playerData,
    const std::vector<std::shared_ptr<DreamcastMainNode>>& dreamcastMainNodes
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

void maple_detect(
    uint8_t& mapleEnabledMask,
    DppSettings& currentDppSettings,
    std::vector<DreamcastNodeData>& dcNodes,
    bool rebootNowOnDetect
)
{
    // Time markers for auto detect when anyMapleAutoDetect is true
    static uint64_t autoDetectTimeUs[MAX_DEVICES] = {};
    static uint64_t autoDetectReactionTimeUs = 0;

    uint8_t i = 0;
    for (const DreamcastNodeData& dcNode : dcNodes)
    {
        const DppSettings::PlayerDetectionMode& mode = dcNode.playerDef.detectionMode;

        if (mode > DppSettings::PlayerDetectionMode::kAutoThreshold && autoDetectTimeUs[i] == 0)
        {
            const uint8_t playerIdx = dcNode.playerDef.index;
            if (dcNode.playerDef.autoDetectOnly)
            {
                // Was disconnected, react on connection
                if (dcNode.mainNode->isDeviceDetected())
                {
                    mapleEnabledMask |= (1 << playerIdx);
                    autoDetectTimeUs[i] = time_us_64();
                    autoDetectReactionTimeUs = (autoDetectTimeUs[i] + 500000);

                    if (mode == DppSettings::PlayerDetectionMode::kAutoStatic)
                    {
                        // Update settings which will be saved later
                        currentDppSettings.playerDetectionModes[playerIdx] = DppSettings::PlayerDetectionMode::kEnable;
                    }
                }
            }
            else if (mode != DppSettings::PlayerDetectionMode::kAutoDynamicNoDisable)
            {
                // Was connected, react on disconnect
                if (!dcNode.mainNode->isDeviceDetected())
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

        if (DppSettings::getInitialSettings() != currentDppSettings)
        {
            // This should cause a reboot
            currentDppSettings.save();
        }

        watchdog_reboot(0, 0, 0);
    }
}
