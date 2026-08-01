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

#include "host_setup.hpp"

#include <hardware/watchdog.h>
#include <hardware/clocks.h>

#include <pico/stdlib.h>
#include <pico/multicore.h>

#include "hal/Usb/usb_interface.hpp"
#include "hal/Usb/client_usb_interface.h"

#include "MaplePassthroughTtyCommandHandler.hpp"
#include "FlycastTtyCommandHandler.hpp"
#include "SystemTtyCommandHandler.hpp"

#include "MapleWebUsbCommandHandler.hpp"
#include "FlycastWebUsbCommandHandler.hpp"
#include "SettingsWebUsbCommandHandler.hpp"
#include "SystemWebUsbCommandHandler.hpp"

#include "PicoIdentification.hpp"
#include "PicoSystemDiagnostics.hpp"
#include "CriticalSectionMutex.hpp"
#include "Mutex.hpp"
#include "Clock.hpp"

#include <unordered_set>
#include <atomic>

#define MAX_DEVICES (DppSettings::kNumPlayers)
const uint32_t WATCHDOG_MAPLE_AUTO_DETECT_MAGIC = 0xEA68D4;
const uint8_t MAPLE_HOST_ADDRESSES[MAX_DEVICES] = {0x00, 0x40, 0x80, 0xC0};
const uint32_t MAPLE_PINS[MAX_DEVICES] = {P1_BUS_START_PIN, P2_BUS_START_PIN, P3_BUS_START_PIN, P4_BUS_START_PIN};
const uint32_t MAPLE_DIR_PINS[MAX_DEVICES] = {P1_DIR_PIN, P2_DIR_PIN, P3_DIR_PIN, P4_DIR_PIN};

static Clock gClock;

// Initialize and enable the hardware watchdog for shared use between cores.
static void heartbeat_setup()
{
    static constexpr uint32_t kSharedWatchdogTimeoutMs = 1000; // 1 second
    watchdog_enable(kSharedWatchdogTimeoutMs, true);
}

void heartbeat()
{
    // Updated whenever core0 does a loop for watchdog check in core1
    static std::atomic<bool> core0Alive = true;

    switch(get_core_num())
    {
        case 0:
            core0Alive.store(true, std::memory_order_relaxed);
            break;

        case 1:
            // check value and reset to false in one operation
            // (relaxed because it doesn't need to synchronize with other data)
            if (core0Alive.exchange(false, std::memory_order_relaxed))
            {
                // We're both alive
                watchdog_update();
            }
            break;

        default:
            // Invalid
            break;
    }
}

static std::map<uint8_t, DreamcastNodeData> setup_dreamcast_nodes(const std::vector<PlayerDefinition>& playerDefs)
{
    std::map<uint8_t, DreamcastNodeData> dcNodeData;

    static CriticalSectionMutex screenMutexes[MAX_DEVICES];
    DreamcastControllerObserver** observers = get_usb_controller_observers();
    static Mutex schedulerMutexes[MAX_DEVICES];
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
            .clock = gClock,
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
    static PicoSystemDiagnostics picoSystemDiagnostics;
    ttyParser->addTtyCommandHandler(std::make_shared<SystemTtyCommandHandler>(
        picoIdentification,
        picoSystemDiagnostics,
        gClock,
        dcNodes
    ));

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
    std::shared_ptr<SystemWebUsbCommandHandler> systemWebUsbCommandHandler =
        std::make_shared<SystemWebUsbCommandHandler>(picoIdentification, gClock, dcNodes);
    webusb_add_parser(systemWebUsbCommandHandler);
    std::shared_ptr<SettingsWebUsbCommandHandler> settingsWebUsbCommandHandler = std::make_shared<SettingsWebUsbCommandHandler>();
    webusb_add_parser(settingsWebUsbCommandHandler);

    return ttyParser;
}

static uint8_t mapleEnabledMask = 0;
static uint8_t mapleDetectedMask = 0;
static DppSettings::PlayerDetectionMode mapleDetectUpdatedModes[DppSettings::kNumPlayers];
static bool maplePlayerModesUpdated = false;

static void maple_detect_init(const std::map<uint8_t, DreamcastNodeData>& dcNodes)
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

void maple_detect(const std::map<uint8_t, DreamcastNodeData>& dcNodes, bool rebootNowOnDetect)
{
    // Time markers for auto detect when anyMapleAutoDetect is true
    static uint64_t autoDetectReactionTimeUs = 0;

    for (const std::pair<const uint8_t, DreamcastNodeData>& dcNode : dcNodes)
    {
        const DppSettings::PlayerDetectionMode& mode = dcNode.second.playerDef->detectionMode;
        const uint8_t playerIdx = dcNode.first;
        const uint8_t playerMask = (1 << playerIdx);
        const bool detected = dcNode.second.mainNode->isDeviceDetected();

        if (detected)
        {
            mapleDetectedMask |= playerMask;
        }
        else
        {
            mapleDetectedMask &= ~playerMask;
        }

        if (mode > DppSettings::PlayerDetectionMode::kAutoThreshold)
        {
            if (dcNode.second.playerDef->autoDetectOnly)
            {
                // Was disconnected on boot, react on connection
                if (detected && (mapleEnabledMask & playerMask) == 0)
                {
                    mapleEnabledMask |= playerMask;
                    autoDetectReactionTimeUs = (time_us_64() + 500000);

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
                if (!detected && (mapleEnabledMask & playerMask) != 0)
                {
                    mapleEnabledMask &= ~playerMask;
                    autoDetectReactionTimeUs = (time_us_64() + 500000);
                }
            }
        }
    }

    watchdog_hw->scratch[1] = mapleEnabledMask | (mapleDetectedMask << 8);

    if (autoDetectReactionTimeUs > 0 && (rebootNowOnDetect || time_us_64() >= autoDetectReactionTimeUs))
    {
        usb_stop();

        watchdog_hw->scratch[0] = WATCHDOG_MAPLE_AUTO_DETECT_MAGIC;

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

void dpp_hw_init(void (*core1Entry)(), std::map<uint8_t, DreamcastNodeData>& dcNodes, bool& runtimeAutoDetect)
{
    set_sys_clock_khz(CPU_FREQ_KHZ, true);

    const bool mapleRebootDetected = (watchdog_hw->scratch[0] == WATCHDOG_MAPLE_AUTO_DETECT_MAGIC);
    const bool settingsRebootDetected = (watchdog_hw->scratch[0] == DppSettings::WATCHDOG_SETTINGS_UPDATED_MAGIC);
    const bool usbCommandRebootDetected = (watchdog_hw->scratch[0] == WATCHDOG_USB_REBOOT_MAGIC);
    const bool rebootDetected = (mapleRebootDetected || settingsRebootDetected || usbCommandRebootDetected);
    // On reboot, check scratch[1] value for previously detected controllers
    const int32_t prevDetectMask = rebootDetected ? watchdog_hw->scratch[1] : 0;

    // These values are no longer needed
    watchdog_hw->scratch[0] = 0;
    watchdog_hw->scratch[1] = 0;

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

    // On auto detect reboot, use lower 8 bits of mask
    // On any other reboot use bits 8 to 15
    int32_t mask = mapleRebootDetected ? 1 : (1 << 8);

    // Enable whatever USB gamepads that need to be enabled from settings or previousDetectMask
    for (uint8_t i = 0; i < MAX_DEVICES; ++i, mask <<= 1)
    {
        if (
            currentDppSettings.playerDetectionModes[i] == DppSettings::PlayerDetectionMode::kEnable ||
            (
                currentDppSettings.playerDetectionModes[i] != DppSettings::PlayerDetectionMode::kDisable &&
                (mask & prevDetectMask) != 0
            )
        )
        {
            set_usb_descriptor_gamepad_en(i, true);
        }
    }

    std::vector<PlayerDefinition> playerDefs;
    playerDefs.reserve(MAX_DEVICES);
    std::unordered_set<int> autoDetectDevs;
    runtimeAutoDetect = false;

    for (uint8_t i = 0; i < MAX_DEVICES; ++i)
    {
        const bool usbEnabled = is_usb_descriptor_gamepad_en(i);
        const DppSettings::PlayerDetectionMode playerDetectionMode = currentDppSettings.playerDetectionModes[i];
        const bool autoDetect = (playerDetectionMode > DppSettings::PlayerDetectionMode::kAutoThreshold);

        if (usbEnabled || autoDetect)
        {
            PlayerDefinition playerDef;

            playerDef.index = i;
            playerDef.mapleHostAddr = MAPLE_HOST_ADDRESSES[i];
            playerDef.gpioA = currentDppSettings.gpioA[i];
            playerDef.gpioDir =  currentDppSettings.gpioDir[i];
            playerDef.dirOutHigh = currentDppSettings.gpioDirOutputHigh[i];
            playerDef.detectionMode = playerDetectionMode;
            playerDef.autoDetectOnly = !usbEnabled;

            if (autoDetect)
            {
                if (!is_usb_descriptor_gamepad_en(i))
                {
                    // Auto detect is enabled and the gamepad is currently disabled
                    if (playerDetectionMode > DppSettings::PlayerDetectionMode::kAutoDynamicThreshold)
                    {
                        autoDetectDevs.insert(i);
                    }
                    runtimeAutoDetect = true;
                }
                else if (playerDetectionMode == DppSettings::PlayerDetectionMode::kAutoDynamic)
                {
                    // Auto, Dynamic detect is enabled and gamepad is currently enabled
                    runtimeAutoDetect = true;
                }
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

    // Create mutexes that persist past the end of this function call
    static Mutex fileMutex;
    static Mutex cdcStdioMutex;
    static Mutex webusbMutex;
    usb_init(
        &fileMutex,
        &cdcStdioMutex,
        &webusbMutex,
        currentDppSettings.usbLedGpio,
        currentDppSettings.simpleUsbLedGpio
    );

    // Enable heartbeat watchdog
    heartbeat_setup();

    multicore_launch_core1(core1Entry);

    if (!autoDetectDevs.empty() && !rebootDetected && !dcNodes.empty())
    {
        // Run for 3.5 seconds or until all auto devices are detected (older VMUs may have 3 second beep)
        const uint64_t endTime = time_us_64() + 3500000;
        while (time_us_64() < endTime && !autoDetectDevs.empty())
        {
            for (const std::pair<const uint8_t, DreamcastNodeData>& dcNode : dcNodes)
            {
                if (dcNode.second.mainNode->isDeviceDetected())
                {
                    autoDetectDevs.erase(dcNode.second.playerDef->index);
                }
            }

            // Signal core 0 liveness to shared watchdog
            heartbeat();
        }

        maple_detect(dcNodes, true);
    }

    usb_start();
}
