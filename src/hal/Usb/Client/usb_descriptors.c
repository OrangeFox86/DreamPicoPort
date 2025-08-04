/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 * Modifications are (c) 2022-2025 James Smith of OrangeFox86
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "tusb.h"
#include "usb_descriptors.h"
#include "class/hid/hid_device.h"
#include "pico/unique_id.h"
#include "configuration.h"
#include <string.h>

static uint8_t numberOfGamepads = MAX_NUMBER_OF_USB_GAMEPADS;

void set_usb_descriptor_number_of_gamepads(uint8_t num)
{
    if (num > MAX_NUMBER_OF_USB_GAMEPADS)
    {
        num = MAX_NUMBER_OF_USB_GAMEPADS;
    }
    numberOfGamepads = num;
}

uint8_t get_usb_descriptor_number_of_gamepads()
{
    return numberOfGamepads;
}

#undef TUD_HID_REPORT_DESC_GAMEPAD

#define GET_NUM_BUTTONS(numPlayers, playerIdx) ((numPlayers == 1) ? 32 : (31 - playerIdx))

// Tweak the gamepad descriptor so that the minimum value on analog controls is -128 instead of -127
#define TUD_HID_REPORT_DESC_GAMEPAD(numPlayers, playerIdx) \
  HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP     )                 ,\
  HID_USAGE      ( HID_USAGE_DESKTOP_GAMEPAD  )                 ,\
  HID_COLLECTION ( HID_COLLECTION_APPLICATION )                 ,\
    HID_REPORT_ID( GAMEPAD_MAIN_REPORT_ID )                     \
    /* 8 bit X, Y */ \
    HID_USAGE_PAGE     ( HID_USAGE_PAGE_DESKTOP                 ) ,\
    HID_USAGE          ( HID_USAGE_DESKTOP_X                    ) ,\
    HID_USAGE          ( HID_USAGE_DESKTOP_Y                    ) ,\
    HID_LOGICAL_MIN    ( MIN_ANALOG_VALUE                       ) ,\
    HID_LOGICAL_MAX    ( MAX_ANALOG_VALUE                       ) ,\
    HID_REPORT_COUNT   ( 2                                      ) ,\
    HID_REPORT_SIZE    ( 8                                      ) ,\
    HID_INPUT          ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,\
    /* 8 bit Z, Rz */ \
    HID_USAGE_PAGE     ( HID_USAGE_PAGE_DESKTOP                 ) ,\
    HID_USAGE          ( HID_USAGE_DESKTOP_Z                    ) ,\
    HID_USAGE          ( HID_USAGE_DESKTOP_RZ                   ) ,\
    HID_LOGICAL_MIN    ( MIN_TRIGGER_VALUE                      ) ,\
    HID_LOGICAL_MAX    ( MAX_TRIGGER_VALUE                      ) ,\
    HID_REPORT_COUNT   ( 2                                      ) ,\
    HID_REPORT_SIZE    ( 8                                      ) ,\
    HID_INPUT          ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,\
    /* 8 bit Rx, Ry */ \
    HID_USAGE_PAGE     ( HID_USAGE_PAGE_DESKTOP                 ) ,\
    HID_USAGE          ( HID_USAGE_DESKTOP_RX                   ) ,\
    HID_USAGE          ( HID_USAGE_DESKTOP_RY                   ) ,\
    HID_LOGICAL_MIN    ( MIN_ANALOG_VALUE                       ) ,\
    HID_LOGICAL_MAX    ( MAX_ANALOG_VALUE                       ) ,\
    HID_REPORT_COUNT   ( 2                                      ) ,\
    HID_REPORT_SIZE    ( 8                                      ) ,\
    HID_INPUT          ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,\
    /* 8 bit DPad/Hat Button Map  */ \
    HID_USAGE_PAGE     ( HID_USAGE_PAGE_DESKTOP                 ) ,\
    HID_USAGE          ( HID_USAGE_DESKTOP_HAT_SWITCH           ) ,\
    HID_LOGICAL_MIN    ( 1                                      ) ,\
    HID_LOGICAL_MAX    ( 8                                      ) ,\
    HID_PHYSICAL_MIN   ( 0                                      ) ,\
    HID_PHYSICAL_MAX_N ( 315, 2                                 ) ,\
    HID_REPORT_COUNT   ( 1                                      ) ,\
    HID_REPORT_SIZE    ( 8                                      ) ,\
    HID_INPUT          ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,\
    /* Up to 32 bit Button Map (less than 32 to index players on some systems) */ \
    HID_USAGE_PAGE     ( HID_USAGE_PAGE_BUTTON                  ) ,\
    HID_USAGE_MIN      ( 1                                      ) ,\
    HID_USAGE_MAX      ( GET_NUM_BUTTONS(numPlayers, playerIdx) ) ,\
    HID_LOGICAL_MIN    ( 0                                      ) ,\
    HID_LOGICAL_MAX    ( 1                                      ) ,\
    HID_REPORT_COUNT   ( GET_NUM_BUTTONS(numPlayers, playerIdx) ) ,\
    HID_REPORT_SIZE    ( 1                                      ) ,\
    HID_INPUT          ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,\
    /* To pad things out to exactly 12 bytes */ \
    HID_USAGE_PAGE_N   ( HID_USAGE_PAGE_VENDOR, 2               ) ,\
    HID_USAGE          ( 0x01                                   ) ,\
    HID_USAGE_MIN      ( 1                                      ) ,\
    HID_USAGE_MAX      ( 8 + (32 - GET_NUM_BUTTONS(numPlayers, playerIdx))) ,\
    HID_LOGICAL_MIN    ( 0                                      ) ,\
    HID_LOGICAL_MAX    ( 1                                      ) ,\
    HID_REPORT_COUNT   ( 8 + (32 - GET_NUM_BUTTONS(numPlayers, playerIdx))) ,\
    HID_REPORT_SIZE    ( 1                                      ) ,\
    HID_INPUT          ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,\
  HID_COLLECTION_END \

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0201, // at least 2.1 or 3.x for BOS & webUSB

    // Use Interface Association Descriptor (IAD) for CDC
    // As required by USB Specs IAD's subclass must be common class (2) and protocol must be IAD (1)
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    // VID 1209 comes from https://pid.codes/
    // PID 2F07 is a subassignment granted through github
    // https://github.com/pidcodes/pidcodes.github.com/blob/74f95539d2ad737c1ba2871eeb25b3f5f5d5c790/1209/2F07/index.md
    .idVendor           = 0x1209,
    .idProduct          = 0x2F07,

    .bcdDevice          = 0x0103,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const *tud_descriptor_device_cb(void) {
    return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// HID Report Descriptor
//--------------------------------------------------------------------+

uint8_t desc_hid_report[] =
{
    TUD_HID_REPORT_DESC_GAMEPAD(MAX_NUMBER_OF_USB_GAMEPADS, 0)
};

// Invoked when received GET HID REPORT DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    if (instance < numberOfGamepads)
    {
        uint8_t buff[] = {TUD_HID_REPORT_DESC_GAMEPAD(numberOfGamepads, instance)};
        memcpy(desc_hid_report, buff, sizeof(desc_hid_report));
        return desc_hid_report;
    }

    return NULL;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

#if USB_MSC_ENABLED
    #define MSC_DESC_LEN TUD_MSC_DESC_LEN
#else
    #define MSC_DESC_LEN 0
#endif

#if USB_CDC_ENABLED
    #define CDC_DESC_LEN TUD_CDC_DESC_LEN
#else
    #define CDC_DESC_LEN 0
#endif

#if USB_WEBUSB_ENABLED
    #define WEBUSB_DESC_LEN (NUM_ITF_WEBUSB * TUD_VENDOR_DESC_LEN)
#else
    #define WEBUSB_DESC_LEN 0
#endif

#define GET_CONFIG_LEN(numGamepads) (TUD_CONFIG_DESC_LEN + (numGamepads * TUD_HID_DESC_LEN) + CDC_DESC_LEN + MSC_DESC_LEN + WEBUSB_DESC_LEN)

// Endpoint definitions (must all be unique)
#define EPIN_GAMEPAD1   (0x84)
#define EPIN_GAMEPAD2   (0x83)
#define EPIN_GAMEPAD3   (0x82)
#define EPIN_GAMEPAD4   (0x81)
#define EPOUT_MSC       (0x05)
#define EPIN_MSC        (0x85)
#define EPIN_CDC_NOTIF  (0x86)
#define EPOUT_CDC       (0x07)
#define EPIN_CDC        (0x87)
#define EPOUT_WEBUSB1   (0x08)
#define EPIN_WEBUSB1    (0x88)
#define EPOUT_WEBUSB2   (0x09)
#define EPIN_WEBUSB2    (0x89)

#define PLAYER_TO_STR_IDX(player) (player + 4)

uint8_t player_to_epin(uint8_t player)
{
    switch (player)
    {
        case 0: return EPIN_GAMEPAD1;
        case 1: return EPIN_GAMEPAD2;
        case 2: return EPIN_GAMEPAD3;
        default:
        case 3: return EPIN_GAMEPAD4;
    }
}

#define CONFIG_HEADER(numGamepads) \
    TUD_CONFIG_DESCRIPTOR(1, ITF_COUNT(numGamepads), 0, GET_CONFIG_LEN(numGamepads), TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 500)

#define GAMEPAD_CONFIG_DESC(itfNum, strIdx, endpt) \
    TUD_HID_DESCRIPTOR(itfNum, strIdx, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report), endpt, GAMEPAD_REPORT_SIZE, 1)

// Only doing transfer at full speed since each file will only be about 128KB, max of 8 files
#define MSC_DESCRIPTOR() TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 8, EPOUT_MSC, EPIN_MSC, 64)

#define CDC_DESCRIPTOR() TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 9, EPIN_CDC_NOTIF, 8, EPOUT_CDC, EPIN_CDC, 64)

#define WEBUSB1_DESCRIPTOR() TUD_VENDOR_DESCRIPTOR(ITF_NUM_WEBUSB1, 10, EPOUT_WEBUSB1, EPIN_WEBUSB1, 64)

#define WEBUSB2_DESCRIPTOR() TUD_VENDOR_DESCRIPTOR(ITF_NUM_WEBUSB2, 11, EPOUT_WEBUSB2, EPIN_WEBUSB2, 64)

// This is setup with the maximum amount of data needed for the description, and it is updated in
// tud_descriptor_configuration_cb() before being sent to the USB host
uint8_t desc_configuration[] =
{
    CONFIG_HEADER(MAX_NUMBER_OF_USB_GAMEPADS),

    // *************************************************************************
    // * Gamepad Descriptors                                                   *
    // *************************************************************************

    GAMEPAD_CONFIG_DESC(0, PLAYER_TO_STR_IDX(0), EPIN_GAMEPAD1),
    GAMEPAD_CONFIG_DESC(1, PLAYER_TO_STR_IDX(1), EPIN_GAMEPAD2),
    GAMEPAD_CONFIG_DESC(2, PLAYER_TO_STR_IDX(2), EPIN_GAMEPAD3),
    GAMEPAD_CONFIG_DESC(3, PLAYER_TO_STR_IDX(3), EPIN_GAMEPAD4),

    // *************************************************************************
    // * Storage Device Descriptor                                             *
    // *************************************************************************

#if USB_MSC_ENABLED
    MSC_DESCRIPTOR(),
#endif

    // *************************************************************************
    // * Communication Device Descriptor  (for debug messaging)                *
    // *************************************************************************

#if USB_CDC_ENABLED
    CDC_DESCRIPTOR(),
#endif

    // *************************************************************************
    // * WebUSB                                                                *
    // *************************************************************************

#if USB_WEBUSB_ENABLED
    WEBUSB1_DESCRIPTOR(),
    WEBUSB2_DESCRIPTOR()
#endif
};

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void) index; // for multiple configurations

    // Build the config based on number of players
    uint32_t offset = 0;

    uint8_t header[] = {
        CONFIG_HEADER(numberOfGamepads)
    };
    memcpy(&desc_configuration[offset], header, sizeof(header));
    offset += sizeof(header);

    for (uint8_t i = 0; i < numberOfGamepads; ++i)
    {
        uint8_t gpConfig[] = {
            GAMEPAD_CONFIG_DESC(i, PLAYER_TO_STR_IDX(i), player_to_epin(i))
        };
        memcpy(&desc_configuration[offset], gpConfig, sizeof(gpConfig));
        offset += sizeof(gpConfig);
    }

#if USB_MSC_ENABLED
    uint8_t mscConfig[] = {
        MSC_DESCRIPTOR()
    };
    memcpy(&desc_configuration[offset], mscConfig, sizeof(mscConfig));
    offset += sizeof(mscConfig);
#endif

#if USB_CDC_ENABLED
    uint8_t cdcConfig[] = {
        CDC_DESCRIPTOR()
    };
    memcpy(&desc_configuration[offset], cdcConfig, sizeof(cdcConfig));
    offset += sizeof(cdcConfig);
#endif

#if USB_WEBUSB_ENABLED
    uint8_t webusbConfig[] = {
        WEBUSB1_DESCRIPTOR(),
        WEBUSB2_DESCRIPTOR()
    };
    memcpy(&desc_configuration[offset], webusbConfig, sizeof(webusbConfig));
    offset += sizeof(webusbConfig);
#endif

    return desc_configuration;
}

//--------------------------------------------------------------------+
// BOS Descriptor
//--------------------------------------------------------------------+

/* Microsoft OS 2.0 registry property descriptor
Per MS requirements https://msdn.microsoft.com/en-us/library/windows/hardware/hh450799(v=vs.85).aspx
device should create DeviceInterfaceGUIDs. It can be done by driver and
in case of real PnP solution device should expose MS "Microsoft OS 2.0
registry property descriptor". Such descriptor can insert any record
into Windows registry per device/configuration/interface. In our case it
will insert "DeviceInterfaceGUIDs" multistring property.

GUID is freshly generated and should be OK to use.

https://developers.google.com/web/fundamentals/native-hardware/build-for-webusb/
(Section Microsoft OS compatibility descriptors)
*/

#define BOS_TOTAL_LEN      (TUD_BOS_DESC_LEN + TUD_BOS_WEBUSB_DESC_LEN + TUD_BOS_MICROSOFT_OS_DESC_LEN)

#define MS_OS_20_DESC_LEN 0x152
#define MS_OS_20_FN1_LEN 0xA0
#define MS_OS_20_FN2_LEN 0xA0

// BOS Descriptor is required for webUSB
uint8_t const desc_bos[] =
{
  // total length, number of device caps
  TUD_BOS_DESCRIPTOR(BOS_TOTAL_LEN, 2),

  // Vendor Code, iLandingPage
  TUD_BOS_WEBUSB_DESCRIPTOR(VENDOR_REQUEST_WEBUSB, 1),

  // Microsoft OS 2.0 descriptor
  TUD_BOS_MS_OS_20_DESCRIPTOR(MS_OS_20_DESC_LEN, VENDOR_REQUEST_MICROSOFT)
};

uint8_t const * tud_descriptor_bos_cb(void)
{
  return desc_bos;
}


uint8_t const desc_ms_os_20[] =
{
  // Set header: length, type, windows version, total length
  U16_TO_U8S_LE(0x000A), U16_TO_U8S_LE(MS_OS_20_SET_HEADER_DESCRIPTOR), U32_TO_U8S_LE(0x06030000), U16_TO_U8S_LE(MS_OS_20_DESC_LEN),

  // Configuration subset header: length, type, configuration index, reserved, configuration total length
  U16_TO_U8S_LE(0x0008), U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_CONFIGURATION), 0, 0, U16_TO_U8S_LE(MS_OS_20_DESC_LEN-0x0A),


  // Function Subset header: length, type, first interface, reserved, subset length
  U16_TO_U8S_LE(0x0008), U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_FUNCTION), ITF_NUM_WEBUSB1, 0, U16_TO_U8S_LE(MS_OS_20_FN1_LEN),

  // MS OS 2.0 Compatible ID descriptor: length, type, compatible ID, sub compatible ID
  U16_TO_U8S_LE(0x0014), U16_TO_U8S_LE(MS_OS_20_FEATURE_COMPATBLE_ID), 'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // sub-compatible

  // MS OS 2.0 Registry property descriptor: length, type
  U16_TO_U8S_LE(MS_OS_20_FN1_LEN-0x08-0x14), U16_TO_U8S_LE(MS_OS_20_FEATURE_REG_PROPERTY),
  U16_TO_U8S_LE(0x0007), U16_TO_U8S_LE(0x002A), // wPropertyDataType, wPropertyNameLength and PropertyName "DeviceInterfaceGUIDs\0" in UTF-16
  'D', 0x00, 'e', 0x00, 'v', 0x00, 'i', 0x00, 'c', 0x00, 'e', 0x00, 'I', 0x00, 'n', 0x00, 't', 0x00, 'e', 0x00,
  'r', 0x00, 'f', 0x00, 'a', 0x00, 'c', 0x00, 'e', 0x00, 'G', 0x00, 'U', 0x00, 'I', 0x00, 'D', 0x00, 's', 0x00, 0x00, 0x00,
  U16_TO_U8S_LE(0x0050), // wPropertyDataLength
	//bPropertyData: “{975F44D9-0D08-43FD-8B3E-127CA8AFFF9D}”.
  '{', 0x00, '9', 0x00, '7', 0x00, '5', 0x00, 'F', 0x00, '4', 0x00, '4', 0x00, 'D', 0x00, '9', 0x00, '-', 0x00,
  '0', 0x00, 'D', 0x00, '0', 0x00, '8', 0x00, '-', 0x00, '4', 0x00, '3', 0x00, 'F', 0x00, 'D', 0x00, '-', 0x00,
  '8', 0x00, 'B', 0x00, '3', 0x00, 'E', 0x00, '-', 0x00, '1', 0x00, '2', 0x00, '7', 0x00, 'C', 0x00, 'A', 0x00,
  '8', 0x00, 'A', 0x00, 'F', 0x00, 'F', 0x00, 'F', 0x00, '9', 0x00, 'D', 0x00, '}', 0x00, 0x00, 0x00, 0x00, 0x00,


  // Function Subset header: length, type, first interface, reserved, subset length
  U16_TO_U8S_LE(0x0008), U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_FUNCTION), ITF_NUM_WEBUSB2, 0, U16_TO_U8S_LE(MS_OS_20_FN2_LEN),

  // MS OS 2.0 Compatible ID descriptor: length, type, compatible ID, sub compatible ID
  U16_TO_U8S_LE(0x0014), U16_TO_U8S_LE(MS_OS_20_FEATURE_COMPATBLE_ID), 'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // sub-compatible

  // MS OS 2.0 Registry property descriptor: length, type
  U16_TO_U8S_LE(MS_OS_20_FN2_LEN-0x08-0x14), U16_TO_U8S_LE(MS_OS_20_FEATURE_REG_PROPERTY),
  U16_TO_U8S_LE(0x0007), U16_TO_U8S_LE(0x002A), // wPropertyDataType, wPropertyNameLength and PropertyName "DeviceInterfaceGUIDs\0" in UTF-16
  'D', 0x00, 'e', 0x00, 'v', 0x00, 'i', 0x00, 'c', 0x00, 'e', 0x00, 'I', 0x00, 'n', 0x00, 't', 0x00, 'e', 0x00,
  'r', 0x00, 'f', 0x00, 'a', 0x00, 'c', 0x00, 'e', 0x00, 'G', 0x00, 'U', 0x00, 'I', 0x00, 'D', 0x00, 's', 0x00, 0x00, 0x00,
  U16_TO_U8S_LE(0x0050), // wPropertyDataLength
	//bPropertyData: “{975F44D9-0D08-43FD-8B3E-127CA8AFFF9D}”.
  '{', 0x00, '9', 0x00, '7', 0x00, '5', 0x00, 'F', 0x00, '4', 0x00, '4', 0x00, 'D', 0x00, '9', 0x00, '-', 0x00,
  '0', 0x00, 'D', 0x00, '0', 0x00, '8', 0x00, '-', 0x00, '4', 0x00, '3', 0x00, 'F', 0x00, 'D', 0x00, '-', 0x00,
  '8', 0x00, 'B', 0x00, '3', 0x00, 'E', 0x00, '-', 0x00, '1', 0x00, '2', 0x00, '7', 0x00, 'C', 0x00, 'A', 0x00,
  '8', 0x00, 'A', 0x00, 'F', 0x00, 'F', 0x00, 'F', 0x00, '9', 0x00, 'E', 0x00, '}', 0x00, 0x00, 0x00, 0x00, 0x00
};

TU_VERIFY_STATIC(sizeof(desc_ms_os_20) == MS_OS_20_DESC_LEN, "Incorrect size");

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// array of pointer to string descriptors
char const *string_desc_arr[] =
{
    (const char[]) {0x09, 0x04}, // 0: is supported language is English (0x0409)
    "OrangeFox86",               // 1: Manufacturer
    "DreamPicoPort",             // 2: Product
    NULL,                        // 3: Serial (special case; get pico serial)
    "DreamPicoPort A",           // 4: Gamepad 1
    "DreamPicoPort B",           // 5: Gamepad 2
    "DreamPicoPort C",           // 6: Gamepad 3
    "DreamPicoPort D",           // 7: Gamepad 4
    "DreamPicoPort MSC",         // 8: Mass Storage Class
    "DreamPicoPort CDC",         // 9: Communication Device Class
    "DreamPicoPort WebUSB1",     // 10: WebUSB1 interface
    "DreamPicoPort WebUSB2",     // 11: WebUSB2 interface
};

static uint16_t _desc_str[32] = {};

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void) langid;

    uint8_t chr_count;
    char buffer[32] = {0};

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        // Convert ASCII string into UTF-16

        if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))) return NULL;

        const char *str = string_desc_arr[index];

        if (index == PLAYER_TO_STR_IDX(0) && numberOfGamepads == 1)
        {
            // Special case - if there is only 1 controller, change the label
            str = "DreamPicoPort";
        }
        else if (str == NULL)
        {
            if (index == 3)
            {
                // Special case: try to get pico serial number
                pico_get_unique_board_id_string(buffer, sizeof(buffer));
                if (buffer[0] != '\0')
                {
                    str = buffer;
                }
                else
                {
                    // Something failed, have host assign serial
                    return NULL;
                }
            }
            else
            {
                return NULL;
            }
        }

        // Cap at max char
        chr_count = strlen(str);
        if (chr_count > 31) chr_count = 31;

        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }
    }

    // first byte is length (including header), second byte is string type
    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);

    return _desc_str;
}
