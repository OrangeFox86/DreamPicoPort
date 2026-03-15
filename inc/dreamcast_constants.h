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

#pragma once

// Device functions
#define DEVICE_FN_CONTROLLER    0x00000001
#define DEVICE_FN_STORAGE       0x00000002
#define DEVICE_FN_LCD           0x00000004
#define DEVICE_FN_TIMER         0x00000008
#define DEVICE_FN_AUDIO_INPUT   0x00000010
#define DEVICE_FN_AR_GUN        0x00000020
#define DEVICE_FN_KEYBOARD      0x00000040
#define DEVICE_FN_GUN           0x00000080
#define DEVICE_FN_VIBRATION     0x00000100
#define DEVICE_FN_MOUSE         0x00000200
#define DEVICE_FN_EXMEDIA       0x00000400
#define DEVICE_FN_CAMERA        0x00000800

//! Enumerates all of the valid commands for Dreamcast devices
enum DreamcastCommand
{
    COMMAND_DEVICE_INFO_REQUEST = 0x01,
    COMMAND_EXT_DEVICE_INFO_REQUEST = 0x02,
    COMMAND_RESET = 0x03,
    COMMAND_SHUTDOWN = 0x04,
    COMMAND_RESPONSE_DEVICE_INFO = 0x05,
    COMMAND_RESPONSE_EXT_DEVICE_INFO = 0x06,
    COMMAND_RESPONSE_ACK = 0x07,
    COMMAND_RESPONSE_DATA_XFER = 0x08,
    COMMAND_GET_CONDITION = 0x09,
    COMMAND_GET_MEMORY_INFORMATION = 0x0A,
    COMMAND_BLOCK_READ = 0x0B,
    COMMAND_BLOCK_WRITE = 0x0C,
    COMMAND_GET_LAST_ERROR = 0x0D,
    COMMAND_SET_CONDITION = 0x0E,
    COMMAND_RESPONSE_AR_ERROR = 0xF9,
    COMMAND_RESPONSE_LCD_ERROR = 0xFA,
    COMMAND_RESPONSE_FILE_ERROR = 0xFB,
    COMMAND_RESPONSE_REQUEST_RESEND = 0xFC,
    COMMAND_RESPONSE_UNKNOWN_COMMAND = 0xFD,
    COMMAND_RESPONSE_FUNCTION_CODE_NOT_SUPPORTED = 0xFE,
    COMMAND_INVALID = 0xFF
};

#define EXPECTED_DEVICE_INFO_PAYLOAD_WORDS 28