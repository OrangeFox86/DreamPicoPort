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

struct DppSettings
{
    //! Number of players supported
    static const uint8_t kNumPlayers = 4;

    //! Enumerates the different player detection modes
    enum class PlayerDetectionMode : std::uint8_t
    {
        kDisable = 0,   //!< No auto detection, always disabled
        kEnable,        //!< No auto detection, always enabled
        kAutoThreshold = kEnable, //!< Any value greater than this value is automatic detection
        kAutoDynamic,   //!< Auto detection, disabling and enabling as device is detected or removed
        kAutoStatic     //!< Initially disabled, switch to kEnable and save settings once detected
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

    //! Initializes and loads settings
    //! @pre must be called before interrupts or core 1 is started
    //! @return loaded settings
    static const DppSettings& initialize();

    //! @return the settings loaded on initialize()
    static const DppSettings& getInitialSettings();

    //! Save settings to flash and reboots system
    //! @pre this must be called from core 0!
    void save();

    //! @pre DppSettings::initialize() must have been called
    //! @return the offset address in flash where settings are located
    static inline uint32_t getSettingsOffsetAddr()
    {
        return kSettingsOffsetAddr;
    }

    bool operator==(const DppSettings&) const = default;
    bool operator!=(const DppSettings&) const = default;

private:
    //! Offset address of settings within flash
    static uint32_t kSettingsOffsetAddr;
    //! The loaded settings on initialize()
    static DppSettings kLoadedSettings;
};
