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

#include <stdint.h>

#include <atomic>

#include "I2cDevice.hpp"

class Ssd1309 : public I2cDevice
{
public:
    enum ByteType : uint8_t
    {
        CONTROL_BYTE = 0x00,
        DATA_BYTE = 0x40
    };

    enum ControlByteType : uint8_t
    {
        CONTROL_POWER_OFF = 0xAE,
        CONTROL_POWER_ON = 0xAF
    };

    // Unless specified, data is written from left to right, top down
    enum DataFormat
    {
        // Most sig bit of each byte is bottom of each column of an 8-bit high row
        EIGHT_BIT_ROWS,
        // Most sig bit of first byte is bottom left of image
        ONE_BIT_ROWS_BOTTOM_UP
    };

    enum class SendingSequence
    {
        NONE = 0,
        CONTROL,
        POWER,
        SCREEN
    };

    enum class MaskType
    {
        NONE = 0,
        OR,
        AND,
        XOR
    };

    struct Icon
    {
        const uint8_t* image;
        uint32_t width;
        uint32_t height;
        DataFormat dataFormat;
    };

public:
    Ssd1309(uint8_t mwAddr = 0x3C);
    bool isPowered();
    void queuePower(bool on);
    bool clearScreen();
    //! Origin X and Y make up the points at the top-left of the icon
    bool setScreenData(
        uint32_t originX,
        uint32_t originY,
        const uint8_t* bytes,
        uint32_t width,
        uint32_t height,
        DataFormat dataFormat,
        MaskType maskType=MaskType::NONE,
        bool invert=false);
    bool setScreenData(
        uint32_t originX,
        uint32_t originY,
        const Icon& icon,
        MaskType maskType=MaskType::NONE,
        bool invert=false);
    //! Resets screen data buffer
    //! @param[in] originX  The number of pixels to move to the right or 0 for origin
    //! @param[in] originY  The number of pixels to move down or 0 for origin
    //! @param[in] width  Pixel width to clear/set
    //! @param[in] height Pixel height to clear/set
    //! @param[in] clr  True to set each pixel to 0 or false to set each pixel to 1
    bool clearScreenData(uint32_t originX, uint32_t originY, uint32_t width, uint32_t height, bool clr=true);
    //! Resets screen area for a given icon
    bool clearScreenData(uint32_t originX, uint32_t originY, const Icon& icon);
    void invertScreenData(uint32_t originX, uint32_t originY, uint32_t width, uint32_t height);
    bool setScreenText(
        uint32_t originX,
        uint32_t originY,
        uint32_t sizeMultiplier,
        const char* text,
        MaskType maskType=MaskType::NONE,
        bool invert=false);
    void queueScreenWrite();
    void queueInitSequence();

private:
    //! @returns the address of this I2C device
    virtual uint8_t getAddress() final;

    //! @param[out] byte  Set to the next byte to send
    //! @param[out] sendStop  Send stop after this byte, send start on next
    //! @returns false if this is the last byte in this sequence
    //! @returns true if there is another byte queued for this sequence
    virtual bool nextByteOut(uint8_t &byte, bool& sendStop) final;

    //! @returns true when data is ready for write
    virtual bool isOutAvailable() final;

    //! Called to notify the device that write failed for the current sequence.
    //! The device is expected to purge its output buffer and optionally start over.
    //! @param[in] abortReason  The reason write failed (see I2C_IC_TX_ABRT_SOURCE definitions)
    virtual void writeFailed(uint32_t abortReason) final;

    //! @param[in] byte  The byte that was read off of the bus
    //! @returns false if this is the last byte to be read
    //! @returns true if another byte is expected
    virtual void nextByteIn(uint8_t byte) final;

    //! @returns number of bytes to read when input is expected or 0
    virtual uint32_t isInExpected() final;

    //! Called to notify the device that read failed for the current sequence.
    //! The device is expected to purge its input buffer and optionally start over.
    //! @param[in] abortReason  The reason read failed (see I2C_IC_TX_ABRT_SOURCE definitions)
    virtual void readFailed(uint32_t abortReason) final;

public:
    static const uint32_t SCREEN_PIXEL_WIDTH = 128;
    static const uint32_t SCREEN_PIXEL_HEIGHT = 64;
    static const uint8_t INIT_CONTROL_SEQUENCE[32];
    static const uint8_t PAGE_CONTROL_SEQUENCE[4];
    static const uint32_t NULL_SEQUENCE_IDX = 0xFFFFFFFF;
    static const uint8_t ASCII_ICONS[96][5];
    static const uint8_t NULL_ASCII_ICON[5];

private:
    const uint8_t mAddr;
    //! 8 x 8 bit rows, 128 columns
    uint8_t mScreen[SCREEN_PIXEL_HEIGHT / 8][SCREEN_PIXEL_WIDTH];
    bool mIsPowered;

    SendingSequence mSendingSequence;
    SendingSequence mSentSequence;

    std::atomic<bool> mControlQueued;
    uint32_t mControlSeqIdx;
    std::atomic<bool> mPowerQueued;
    uint32_t mPowerSeqIdx;
    std::atomic<bool> mScreenQueued;
    std::atomic<bool> mHighPriorityScreenQueued;
    bool mScreenSendData;
    uint32_t mScreenSeqRowIdx;
    uint32_t mScreenSeqColIdx;
    uint32_t mScreenPageSeqIdx;
};