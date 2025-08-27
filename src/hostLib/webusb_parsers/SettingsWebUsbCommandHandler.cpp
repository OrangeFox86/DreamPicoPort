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

#include "SettingsWebUsbCommandHandler.hpp"

#include <cstring>

SettingsWebUsbCommandHandler::SettingsWebUsbCommandHandler() :
    mLoadedSettings(DppSettings::getInitialSettings()),
    mSettings(mLoadedSettings)
{}

static std::uint32_t flip_word_bytes(const int32_t& word)
{
    const std::uint32_t wordU32 = static_cast<std::uint32_t>(word);
    return (wordU32 << 24) | (wordU32 << 8 & 0xFF0000) | (wordU32 >> 8 & 0xFF00) | (wordU32 >> 24);
}

static std::string packSettings(const DppSettings& settings)
{
    std::string settingsData;

    settingsData.reserve(50);
    settingsData.push_back(static_cast<std::uint8_t>(settings.cdcEn ? 1 : 0));
    settingsData.push_back(static_cast<std::uint8_t>(settings.mscEn ? 1 : 0));

    for (std::uint8_t i = 0; i < DppSettings::kNumPlayers; ++i)
    {
        settingsData.push_back(static_cast<std::uint8_t>(settings.playerDetectionModes[i]));
    }

    for (std::uint8_t i = 0; i < DppSettings::kNumPlayers; ++i)
    {
        std::uint32_t val = flip_word_bytes(settings.gpioA[i]);
        settingsData.append(reinterpret_cast<const char*>(&val), sizeof(val));
    }

    for (std::uint8_t i = 0; i < DppSettings::kNumPlayers; ++i)
    {
        std::int32_t val = flip_word_bytes(settings.gpioDir[i]);
        settingsData.append(reinterpret_cast<const char*>(&val), sizeof(val));
    }

    for (std::uint8_t i = 0; i < DppSettings::kNumPlayers; ++i)
    {
        settingsData.push_back(static_cast<std::uint8_t>(settings.gpioDirOutputHigh[i]));
    }

    std::int32_t v = flip_word_bytes(settings.usbLedGpio);
    settingsData.append(reinterpret_cast<const char*>(&v), sizeof(v));

    v = flip_word_bytes(settings.simpleUsbLedGpio);
    settingsData.append(reinterpret_cast<const char*>(&v), sizeof(v));

    return settingsData;
}

void SettingsWebUsbCommandHandler::process(
    const std::uint8_t* payload,
    std::uint16_t payloadLen,
    const std::function<
        void(std::uint8_t responseCmd, const std::list<std::pair<const void*, std::uint16_t>>& payloadList)
    >& responseFn
)
{
    const std::uint8_t* eol = payload + payloadLen;
    const std::uint8_t* iter = payload;

    if (iter >= eol)
    {
        responseFn(kResponseCmdInvalid, {});
        return;
    }

    switch(*iter)
    {
        // Get all loaded settings
        case 'G':
        {
            std::string settingsData = packSettings(mLoadedSettings);
            responseFn(kResponseSuccess, {{settingsData.data(), settingsData.size()}});
        }
        return;

        // Get all locally-stored settings
        case 'g':
        {
            std::string settingsData = packSettings(mSettings);
            responseFn(kResponseSuccess, {{settingsData.data(), settingsData.size()}});
        }
        return;

        // Set USB CDC enabled flag
        case 'C':
        {
            ++iter;
            if (iter >= eol)
            {
                std::uint8_t payload = 0;
                responseFn(kResponseFailure, {{&payload, 1}});
                return;
            }

            mSettings.cdcEn = (*iter != 0);
            responseFn(kResponseSuccess, {});
        }
        return;

        // Set USB MSC enabled flag
        case 'M':
        {
            ++iter;
            if (iter >= eol)
            {
                std::uint8_t payload = 0;
                responseFn(kResponseFailure, {{&payload, 1}});
                return;
            }

            mSettings.mscEn = (*iter != 0);
            responseFn(kResponseSuccess, {});
        }
        return;

        // Player detection mode: P[0-3][PlayerDetectionMode (1 byte)]
        case 'P':
        {
            ++iter;
            if (eol - iter < 2)
            {
                std::uint8_t payload = 0;
                responseFn(kResponseFailure, {{&payload, 1}});
                return;
            }

            std::uint8_t playerIdx = (*iter);
            if (playerIdx >= DppSettings::kNumPlayers)
            {
                std::uint8_t payload = 1;
                responseFn(kResponseFailure, {{&payload, 1}});
                return;
            }

            std::uint8_t playerDetectionMode = (*(iter + 1));
            if (
                playerDetectionMode >=
                static_cast<std::uint8_t>(DppSettings::PlayerDetectionMode::kNumPlayerDetectionModes)
            )
            {
                std::uint8_t payload = 2;
                responseFn(kResponseFailure, {{&payload, 1}});
                return;
            }

            mSettings.playerDetectionModes[playerIdx] =
                static_cast<DppSettings::PlayerDetectionMode>(playerDetectionMode);
            responseFn(kResponseSuccess, {});
        }
        return;

        // Player I/O settings: I[0-3][GPIO A (4 byte)][GPIO DIR (4 byte)][DIR Output HIGH (1 byte)]
        case 'I':
        {
            ++iter;
            if (eol - iter < 10)
            {
                std::uint8_t payload = 0;
                responseFn(kResponseFailure, {{&payload, 1}});
                return;
            }

            std::uint8_t playerIdx = (*iter);
            if (playerIdx >= DppSettings::kNumPlayers)
            {
                std::uint8_t payload = 1;
                responseFn(kResponseFailure, {{&payload, 1}});
                return;
            }

            std::int32_t gpioA;
            memcpy(&gpioA, iter + 1, 4);
            gpioA = flip_word_bytes(gpioA);
            if (!DppSettings::isGpioValid(gpioA))
            {
                std::uint8_t payload = 2;
                responseFn(kResponseFailure, {{&payload, 1}});
                return;
            }

            std::int32_t gpioDir;
            memcpy(&gpioDir, iter + 5, 4);
            gpioDir = flip_word_bytes(gpioDir);
            if (!DppSettings::isGpioValid(gpioDir))
            {
                std::uint8_t payload = 3;
                responseFn(kResponseFailure, {{&payload, 1}});
                return;
            }

            bool dirOutputHigh = ((*(iter + 9)) != 0);

            mSettings.gpioA[playerIdx] = gpioA;
            mSettings.gpioDir[playerIdx] = gpioDir;
            mSettings.gpioDirOutputHigh[playerIdx] = dirOutputHigh;
            responseFn(kResponseSuccess, {});
        }
        return;

        // LED setting L[LED Pin (4 byte)]
        case 'L':
        {
            ++iter;
            if (eol - iter < 4)
            {
                std::uint8_t payload = 0;
                responseFn(kResponseFailure, {{&payload, 1}});
                return;
            }

            std::int32_t usbLedGpio;
            memcpy(&usbLedGpio, iter, 4);
            usbLedGpio = flip_word_bytes(usbLedGpio);
            if (!DppSettings::isGpioValid(usbLedGpio))
            {
                std::uint8_t payload = 1;
                responseFn(kResponseFailure, {{&payload, 1}});
                return;
            }

            mSettings.usbLedGpio = usbLedGpio;
            responseFn(kResponseSuccess, {});
        }
        return;

        // Simple LED setting l[LED Pin (4 byte)]
        case 'l':
        {
            ++iter;
            if (eol - iter < 4)
            {
                std::uint8_t payload = 0;
                responseFn(kResponseFailure, {{&payload, 1}});
                return;
            }

            std::int32_t simpleUsbLedGpio;
            memcpy(&simpleUsbLedGpio, iter, 4);
            simpleUsbLedGpio = flip_word_bytes(simpleUsbLedGpio);
            if (!DppSettings::isGpioValid(simpleUsbLedGpio))
            {
                std::uint8_t payload = 1;
                responseFn(kResponseFailure, {{&payload, 1}});
                return;
            }

            mSettings.simpleUsbLedGpio = simpleUsbLedGpio;
            responseFn(kResponseSuccess, {});
        }
        return;

        // Save all settings and reboot
        case 'S':
        {
            if (mSettings.makeValid())
            {
                // Send response before saving
                responseFn(kResponseSuccess, {});
            }
            else
            {
                // Settings had to be adjusted to be valid, save and respond with adjusted settings
                std::string settingsData = packSettings(mSettings);
                responseFn(kResponseSuccess, {{settingsData.data(), settingsData.size()}});
            }

            // This will save and reboot on core 0 - delay for 100 ms to allow the above message to go out
            mSettings.requestSave(100);
        }
        return;

        // Invalid
        default:
            responseFn(kResponseCmdInvalid, {});
            return;
    }
}
