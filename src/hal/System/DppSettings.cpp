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

#include <cstdint>
#include <cstring>
#include <unordered_set>
#include <optional>

#include <configuration.h>

#include <hal/System/DppSettings.hpp>

#include <pico/stdlib.h>
#include <hardware/flash.h>
#include <hardware/irq.h>
#include <pico/platform.h>
#include <hardware/watchdog.h>
#include <pico/multicore.h>

#include <hal/System/LockGuard.hpp>
#include "CriticalSectionMutex.hpp"

static CriticalSectionMutex gSaveMutex;

static const uint32_t kMagic = 0xA875EBBB;

static const uint32_t kUsbEnableCdcMask = 0x00000001;
static const uint32_t kUsbEnableMscMask = 0x00000002;

struct SettingsMemory
{
	uint32_t magic;
    uint16_t size; // Number of 4-byte words, starting at CRC
    uint16_t invSize;
	uint32_t crc;
    uint32_t usbEn;
    uint8_t playerEnableMode[4];
    int32_t gpioA[4];
    int32_t gpioDir[4];
    uint8_t gpioDirOutputHigh[4];
    int32_t usbLedGpio;
    int32_t simpleUsbLedGpio;
};

static const uint16_t kSettingsMemorySizeBytes = (sizeof(SettingsMemory) - offsetof(SettingsMemory, crc));
static const uint16_t kSettingsMemorySizeWords = (kSettingsMemorySizeBytes / 4);

static_assert(sizeof(SettingsMemory) < FLASH_PAGE_SIZE, "Incorrect size");
static_assert(kSettingsMemorySizeBytes % 4 == 0, "Incorrect size");

static uint32_t __no_inline_not_in_flash_func(get_settings_flash_offset)()
{
    uint8_t txbuf[16] = {0x9f};
    uint8_t rxbuf[16] = {0};
    flash_do_cmd(txbuf, rxbuf, 16);

    // Last flash sector contains all settings
    // For pico 1, this should return (0x200000 - FLASH_SECTOR_SIZE)
    // For pico 2, this should return (0x400000 - FLASH_SECTOR_SIZE)
    return ((1 << rxbuf[3]) - FLASH_SECTOR_SIZE);
}

uint32_t __no_inline_not_in_flash_func(calc_crc32)(const uint32_t* ptr, uint16_t numWords)
{
    uint32_t crc = 0xFFFFFFFF;
    while (numWords-- > 0)
    {
        crc ^= *ptr++;
    }
    return crc;
}

const DppSettings& DppSettings::initialize()
{
    sSettingsOffsetAddr = get_settings_flash_offset();

    const SettingsMemory* settingsMemory = reinterpret_cast<const SettingsMemory*>(
        XIP_BASE + sSettingsOffsetAddr);

    // Ensure valid data
    if (
        settingsMemory->magic != kMagic ||
        ((settingsMemory->size ^ settingsMemory->invSize) != 0xFFFF) ||
        (settingsMemory->size < kSettingsMemorySizeWords) ||
        (calc_crc32(&settingsMemory->crc, settingsMemory->size) != 0)
    )
    {
        // Data in flash is invalid
        return sLoadedSettings;
    }

    sLoadedSettings.cdcEn = ((settingsMemory->usbEn & kUsbEnableCdcMask) > 0);
    sLoadedSettings.mscEn = ((settingsMemory->usbEn & kUsbEnableMscMask) > 0);
    sLoadedSettings.usbLedGpio = settingsMemory->usbLedGpio;
    sLoadedSettings.simpleUsbLedGpio = settingsMemory->simpleUsbLedGpio;
    for (uint8_t i = 0; i < kNumPlayers; ++i)
    {
        sLoadedSettings.playerDetectionModes[i] =
            static_cast<DppSettings::PlayerDetectionMode>(settingsMemory->playerEnableMode[i]);
        sLoadedSettings.gpioA[i] = settingsMemory->gpioA[i];
        sLoadedSettings.gpioDir[i] = settingsMemory->gpioDir[i];
        sLoadedSettings.gpioDirOutputHigh[i] = settingsMemory->gpioDirOutputHigh[i];
    }

    return sLoadedSettings;
}

const DppSettings& DppSettings::getInitialSettings()
{
    return sLoadedSettings;
}

void DppSettings::requestSave(uint32_t delayMs)
{
    LockGuard lock(gSaveMutex);

    if (!sSaveOrClearRequested)
    {
        sSaveRequestedSettings = *this;
        sDelayMs = delayMs;
        sSaveRequestTime = time_us_32();
        sSaveOrClearRequested = true;
    }
}

void DppSettings::requestClear(uint32_t delayMs)
{
    LockGuard lock(gSaveMutex);

    if (!sSaveOrClearRequested)
    {
        sSaveRequestedSettings = std::nullopt;
        sDelayMs = delayMs;
        sSaveRequestTime = time_us_32();
        sSaveOrClearRequested = true;
    }
}

void __no_inline_not_in_flash_func(save_settings_memory)(
    uint32_t offsetAddr,
    std::optional<SettingsMemory> mem,
    uint32_t delayMs
)
{
    // This function must be called by core0, and core1 is stopped to ensure nothing is accessing flash
    multicore_reset_core1();

    // Set all IO to input
    gpio_set_dir_in_masked(0xFFFFFFFF);
    gpio_set_function_masked(0xFFFFFFFF, gpio_function_t::GPIO_FUNC_NULL);

    // Disable all IRQs on this core
    uint32_t savedInterrupts = save_and_disable_interrupts();
    busy_wait_us(50000); // note: DO NOT USE sleep_*() FUNCTIONS WHILE INTERRUPTS ARE DISABLED!

    flash_range_erase(offsetAddr, FLASH_SECTOR_SIZE);

    if (mem.has_value())
    {
        // Create the page since flash_range_program() requires multiple of FLASH_PAGE_SIZE bytes
        uint8_t page[FLASH_PAGE_SIZE];
        memcpy(&page[0], &mem.value(), sizeof(SettingsMemory));
        memset(&page[sizeof(SettingsMemory)], 0xFF, sizeof(page) - sizeof(SettingsMemory));

        // WRITE IT!
        flash_range_program(offsetAddr, page, FLASH_PAGE_SIZE);
    }

    // Wait a moment
    busy_wait_us(50000);

    // Interrupts need to be restored in order to reboot
    restore_interrupts_from_disabled(savedInterrupts);

    // Signal that we're rebooting due to settings update
    watchdog_hw->scratch[0] = DppSettings::WATCHDOG_SETTINGS_UPDATED_MAGIC;

    // Reboot now to apply settings
    watchdog_reboot(0, 0, delayMs);
}

void DppSettings::processSaveRequests(const std::function<void()>& hwStopFn)
{
    if (sSaving || !sSaveOrClearRequested)
    {
        return;
    }

    std::optional<DppSettings> settingsToSave;

    // gSaveMutex locking context
    {
        LockGuard lock(gSaveMutex);

        const uint32_t elapsedMsSinceRequest = (time_us_32() - sSaveRequestTime);

        if (sDelayMs > elapsedMsSinceRequest)
        {
            // Better to just wait for another loop rather than rely on watchdog timeout
            return;
        }

        settingsToSave = sSaveRequestedSettings;
    }

    // Call the supplied function
    if (hwStopFn)
    {
        hwStopFn();
    }

    if (settingsToSave.has_value())
    {
        settingsToSave->save(0);
    }
    else
    {
        // Clear without saving
        save_settings_memory(sSettingsOffsetAddr, std::nullopt, 0);
    }
}

void DppSettings::save(uint32_t delayMs) const
{
    sSaving = true;

    SettingsMemory mem{
        .magic = kMagic,
        .size = kSettingsMemorySizeWords,
        .crc = 0xFFFFFFFF,
        .usbEn = (cdcEn ? kUsbEnableCdcMask : 0) | (mscEn ? kUsbEnableMscMask : 0),
        .usbLedGpio = usbLedGpio,
        .simpleUsbLedGpio = simpleUsbLedGpio
    };
    mem.invSize = mem.size ^ 0xFFFF;
    for (uint8_t i = 0; i < kNumPlayers; ++i)
    {
        mem.playerEnableMode[i] = static_cast<uint8_t>(playerDetectionModes[i]);
        mem.gpioA[i] = gpioA[i];
        mem.gpioDir[i] = gpioDir[i];
        mem.gpioDirOutputHigh[i] = gpioDirOutputHigh[i];
    }
    mem.crc = calc_crc32(&mem.crc + 1, mem.size - 1);
    save_settings_memory(sSettingsOffsetAddr, mem, delayMs);
}

static bool validate_gpio(std::unordered_set<std::uint32_t>& usedIo, std::uint32_t newIo)
{
    if (!DppSettings::isGpioValid(newIo))
    {
        // Invalid
        return false;
    }
    else if (usedIo.find(newIo) != usedIo.end())
    {
        // Already exists
        return false;
    }

    usedIo.insert(newIo);
    return true;
}

static bool validate_gpio(std::unordered_set<std::uint32_t>& usedIo, std::int32_t newIo)
{
    if (newIo >= 0)
    {
        return validate_gpio(usedIo, static_cast<std::uint32_t>(newIo));
    }

    // Negative value is unset and therefore valid
    return true;
}

bool DppSettings::makeValid(bool disablePlayerOnBadGpio)
{
    bool alreadyValid = true;
    std::unordered_set<std::uint32_t> usedIo;

    if (!validate_gpio(usedIo, usbLedGpio))
    {
        usbLedGpio = -1; // disable
        alreadyValid = false;
    }

    if (!validate_gpio(usedIo, simpleUsbLedGpio))
    {
        simpleUsbLedGpio = -1; // disable
        alreadyValid = false;
    }

    // Disable players with invalid GPIO value
    if (disablePlayerOnBadGpio)
    {
        for (std::uint8_t i = 0; i < kNumPlayers; ++i)
        {
            if (gpioA[i] < 0 && playerDetectionModes[i] != PlayerDetectionMode::kDisable)
            {
                playerDetectionModes[i] = PlayerDetectionMode::kDisable;
                alreadyValid = false;
            }
        }
    }

    // Handle overlapping GPIO values
    for (std::uint8_t i = 0; i < kNumPlayers; ++i)
    {
        // Only check player pins if not disabled
        if (playerDetectionModes[i] != PlayerDetectionMode::kDisable && gpioA[i] >= 0)
        {
            int32_t gpioB = gpioA[i] + 1;

            if (!validate_gpio(usedIo, gpioDir[i]))
            {
                // Disable gpioDir
                gpioDir[i] = -1;
                alreadyValid = false;

                if (disablePlayerOnBadGpio)
                {
                    playerDetectionModes[i] = PlayerDetectionMode::kDisable;
                }
            }

            if (!validate_gpio(usedIo, gpioA[i]) || !validate_gpio(usedIo, gpioB))
            {
                // Disable gpioA
                gpioA[i] = -1;
                alreadyValid = false;

                if (disablePlayerOnBadGpio)
                {
                    playerDetectionModes[i] = PlayerDetectionMode::kDisable;
                }
            }

            if (
                static_cast<std::uint8_t>(playerDetectionModes[i]) >=
                static_cast<std::uint8_t>(PlayerDetectionMode::kNumPlayerDetectionModes)
            )
            {
                // This seems like the best option when this setting is invalid
                playerDetectionModes[i] = PlayerDetectionMode::kAutoDynamicNoDisable;
                alreadyValid = false;
            }
        }
    }

    return alreadyValid;
}

bool DppSettings::isGpioValid(std::int32_t gpio)
{
    if (gpio < 0)
    {
        // disabled
        return true;
    }

    return isGpioValid(static_cast<std::uint32_t>(gpio));
}

bool DppSettings::isGpioValid(std::uint32_t gpio)
{
    if (gpio >= NUM_BANK0_GPIOS)
    {
        return false;
    }

    return true;
}

uint32_t DppSettings::sSettingsOffsetAddr = 0;
DppSettings DppSettings::sLoadedSettings;
bool DppSettings::sSaveOrClearRequested = false;
uint32_t DppSettings::sDelayMs = 0;
uint32_t DppSettings::sSaveRequestTime = 0;
std::optional<DppSettings> DppSettings::sSaveRequestedSettings = std::nullopt;
bool DppSettings::sSaving = false;
