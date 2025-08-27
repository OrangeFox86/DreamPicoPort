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

#include "configuration.h"

#include <cstdint>

struct DppSettings
{
    //! Number of players supported
    static const uint8_t kNumPlayers = 4;

    //! Enumerates the different player detection modes
    enum class PlayerDetectionMode : std::uint8_t
    {
        //! No auto detection, always disabled
        kDisable = 0,
        //! No auto detection, always enabled
        kEnable,
        //! Any value greater than this value is automatic detection
        kAutoThreshold = kEnable,
        //! Initially disabled, switch to kEnable and save settings once detected
        kAutoStatic,
        //! Auto detection, disabling and enabling as device is detected or removed
        kAutoDynamic,
        //! Auto detection, enabling as device is detected, only disabled upon power loss
        kAutoDynamicNoDisable,

        //! A count of number of modes
        kNumPlayerDetectionModes
    };

    //! USB CDC enabled flag (default: true)
    bool cdcEn = true;
    //! USB MSC enabled flag (default: false)
    bool mscEn = false;
    //! Detection mode for each player
    PlayerDetectionMode playerDetectionModes[kNumPlayers] = {
        PlayerDetectionMode::kAutoStatic,
        PlayerDetectionMode::kAutoStatic,
        PlayerDetectionMode::kAutoStatic,
        PlayerDetectionMode::kAutoStatic
    };
    //! GPIO number of maple A, maple B is always very next one
    int32_t gpioA[kNumPlayers] = {
        P1_BUS_START_PIN,
        P2_BUS_START_PIN,
        P3_BUS_START_PIN,
        P4_BUS_START_PIN
    };
    //! GPIO number of direction output (-1 to disable)
    int32_t gpioDir[kNumPlayers] = {
        P1_DIR_PIN,
        P2_DIR_PIN,
        P3_DIR_PIN,
        P4_DIR_PIN
    };
    //! true if output is high when currently outputting, false for opposite
    bool gpioDirOutputHigh[kNumPlayers] = {
        DIR_OUT_HIGH,
        DIR_OUT_HIGH,
        DIR_OUT_HIGH,
        DIR_OUT_HIGH
    };
    //! LED GPIO number for USB activity or -1 to disable
    //! When USB connected:
    //!   Default: ON
    //!   When controller key pressed: OFF
    //! When USB disconnected:
    //!   Default: OFF
    //!   When controller key pressed: Flashing quick
    int32_t usbLedGpio = USB_LED_PIN;
    //! LED GPIO number for simple USB activity or -1 to disable
    //! ON when USB connected; OFF when disconnected
    int32_t simpleUsbLedGpio = SIMPLE_USB_LED_PIN;

    //! Initializes and loads settings
    //! @pre must be called before interrupts or core 1 is started
    //! @return loaded settings
    static const DppSettings& initialize();

    //! @return the settings loaded on initialize()
    static const DppSettings& getInitialSettings();

    //! Called from core 1 to request save on core 0
    //! @param[in] delayMs Number of ms to delay before saving
    void requestSave(uint32_t delayMs);

    //! Called from core 1 to request settings clear on core 0
    //! @param[in] delayMs Number of ms to delay before saving
    static void requestClear(uint32_t delayMs);

    //! Processes any save requests
    static void processSaveRequests();

    //! Save settings to flash and reboots system
    //! @pre this must be called from core 0!
    //! @param[in] delayMs Number of milliseconds to delay before rebooting
    void save(uint32_t delayMs = 0) const;

    //! Forces valid settings
    //! @param[in] disablePlayerOnBadGpio Set to true to disable player if its gpio is invalid
    //! @return true iff settings were already valid
    bool makeValid(bool disablePlayerOnBadGpio = false);

    //! @pre DppSettings::initialize() must have been called
    //! @return the offset address in flash where settings are located
    static inline uint32_t getSettingsOffsetAddr()
    {
        return sSettingsOffsetAddr;
    }

    //! @return true iff the given gpio is valid
    static bool isGpioValid(std::int32_t gpio);

    //! @return true iff the given gpio is valid
    static bool isGpioValid(std::uint32_t gpio);

    bool operator==(const DppSettings&) const = default;
    bool operator!=(const DppSettings&) const = default;

private:
    //! Offset address of settings within flash
    static uint32_t sSettingsOffsetAddr;
    //! The loaded settings on initialize()
    static DppSettings sLoadedSettings;
    //! Set to true when save is requested
    static bool sSaveOrClearRequested;
    //! Set to true when the above is also true and clear was requested instead of save
    static bool sClearRequested;
    //! The delay to wait when above is true
    static uint32_t sDelayMs;
    //! The 32-bit save requested time
    static uint32_t sSaveRequestTime;
    //! The settings used to save when above is true
    static DppSettings sSaveRequestedSettings;
    //! Set to true once saving, no further save requests may be made
    static bool sSaving;
};
