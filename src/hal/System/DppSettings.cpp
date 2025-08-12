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

#include <configuration.h>

#include <hal/System/DppSettings.hpp>

#include <pico/stdlib.h>
#include <hardware/flash.h>
#include <hardware/irq.h>
#include <pico/platform.h>
#include <hardware/watchdog.h>
#include <pico/multicore.h>

static const uint32_t kMagic = 0xA875EBBB;

static const uint32_t kUsbEnableCdcMask = 0x00000001;
static const uint32_t kUsbEnableMscMask = 0x00000002;

struct SettingsMemory
{
	uint32_t magic;
    uint16_t size; // Number of words, starting at CRC
    uint16_t invSize;
	uint32_t crc;
    uint32_t usbEn;
	uint8_t pad[FLASH_PAGE_SIZE - (4 * sizeof(uint32_t))];
};

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

DppSettings DppSettings::initialize()
{
    // Defaults
    DppSettings settings = {
        .cdcEn = USB_CDC_ENABLED,
        .mscEn = USB_MSC_ENABLED
    };

    kSettingsOffsetAddr = get_settings_flash_offset();

    const SettingsMemory* settingsMemory = reinterpret_cast<const SettingsMemory*>(
        XIP_BASE + kSettingsOffsetAddr);

    // Ensure valid data
    if (
        settingsMemory->magic != kMagic ||
        ((settingsMemory->size ^ settingsMemory->invSize) != 0xFFFF) ||
        (settingsMemory->size < 2) ||
        (calc_crc32(&settingsMemory->crc, settingsMemory->size) != 0)
    )
    {
        return settings;
    }

    settings.cdcEn = ((settingsMemory->usbEn & kUsbEnableCdcMask) > 0);
    settings.mscEn = ((settingsMemory->usbEn & kUsbEnableMscMask) > 0);

    return settings;
}

static void __not_in_flash_func(save_settings_memory)(const SettingsMemory& mem)
{
    // This function must be called by core0, and core1 is stopped to ensure nothing is accessing flash
    multicore_reset_core1();

    // Set all IO to input
    gpio_set_dir_in_masked(0xFFFFFFFF);
    gpio_set_function_masked(0xFFFFFFFF, gpio_function_t::GPIO_FUNC_NULL);

    // Disable all IRQs on this core
    save_and_disable_interrupts();

    flash_range_erase(DppSettings::getSettingsOffsetAddr(), FLASH_SECTOR_SIZE);

    flash_range_program(
        DppSettings::getSettingsOffsetAddr(),
        (const uint8_t*)&mem,
        sizeof(SettingsMemory)
    );

    // Wait a moment
    sleep_ms(50);

    // Reboot now to apply settings
    watchdog_reboot(0, 0, 0);
}

void DppSettings::save()
{
    SettingsMemory mem{
        .magic = kMagic,
        .size = 0x0002,
        .crc = 0xFFFFFFFF,
        .usbEn = (cdcEn ? kUsbEnableCdcMask : 0) | (mscEn ? kUsbEnableMscMask : 0)
    };
    mem.invSize = mem.size ^ 0xFFFF;
    mem.crc = calc_crc32(&mem.crc + 1, mem.size - 1);
    save_settings_memory(mem);
}

uint32_t DppSettings::kSettingsOffsetAddr = 0;
