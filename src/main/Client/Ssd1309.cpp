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

#include "Ssd1309.hpp"

#include <string.h>
#include <assert.h>

const uint8_t Ssd1309::INIT_CONTROL_SEQUENCE[32] = {
    CONTROL_BYTE, // Start of control sequence
    CONTROL_POWER_OFF,  // turn off oled panel
    0x00,  // set low column address
    0x10,  // set high column address
    0x40,  // set start line address
    0x81,  // set contrast control register
    0x6F,
    0xA0,  // don't flip left-right
    0xA6,  // set normal display
    0xA8,  // set multiplex ratio(1 to 64)
    0x3F,  // 1/64 duty
    0xD3,  // set display offset
    0x00,  // not offset
    0xD5,  // set display clock divide ratio/oscillator frequency
    0xF0,  // set divide ratio
    0xD9,  // set pre-charge period
    0x22,
    0xDA,  // set com pins hardware configuration
    0x12,
    0xDB,  // set vcomh
    0x20,
    0x20,  // page addressing mode (seems like some displays don't support horizontal addressing)
    0x02,
    0xC0,  // Normal scan mode. Scan from Com0 to ComN-1
    0x2E,  // Deactivate scroll
    0xA4,  // Display from RAM
    0x21,  // Set display width
    0, SCREEN_PIXEL_WIDTH - 1,
    0x22,  // Set display height
    0, (SCREEN_PIXEL_HEIGHT / 8) - 1,
};

const uint8_t Ssd1309::PAGE_CONTROL_SEQUENCE[4] = {
    CONTROL_BYTE, 0xB0, 0x00, 0x10
};

const uint8_t Ssd1309::ASCII_ICONS[96][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00},  // (space)
    {0x00, 0x00, 0x5F, 0x00, 0x00},  // !
    {0x00, 0x07, 0x00, 0x07, 0x00},  // "
    {0x14, 0x7F, 0x14, 0x7F, 0x14},  // //
    {0x24, 0x2A, 0x7F, 0x2A, 0x12},  // $
    {0x23, 0x13, 0x08, 0x64, 0x62},  // %
    {0x36, 0x49, 0x55, 0x22, 0x50},  // &
    {0x00, 0x05, 0x03, 0x00, 0x00},  // '
    {0x00, 0x1C, 0x22, 0x41, 0x00},  // (
    {0x00, 0x41, 0x22, 0x1C, 0x00},  // )
    {0x08, 0x2A, 0x1C, 0x2A, 0x08},  // *
    {0x08, 0x08, 0x3E, 0x08, 0x08},  // +
    {0x00, 0x50, 0x30, 0x00, 0x00},  // ,
    {0x08, 0x08, 0x08, 0x08, 0x08},  // -
    {0x00, 0x60, 0x60, 0x00, 0x00},  // .
    {0x20, 0x10, 0x08, 0x04, 0x02},  // /
    {0x3E, 0x51, 0x49, 0x45, 0x3E},  // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00},  // 1
    {0x42, 0x61, 0x51, 0x49, 0x46},  // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31},  // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10},  // 4
    {0x27, 0x45, 0x45, 0x45, 0x39},  // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30},  // 6
    {0x01, 0x71, 0x09, 0x05, 0x03},  // 7
    {0x36, 0x49, 0x49, 0x49, 0x36},  // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E},  // 9
    {0x00, 0x36, 0x36, 0x00, 0x00},  // :
    {0x00, 0x56, 0x36, 0x00, 0x00},  // ;
    {0x00, 0x08, 0x14, 0x22, 0x41},  // <
    {0x14, 0x14, 0x14, 0x14, 0x14},  // =
    {0x41, 0x22, 0x14, 0x08, 0x00},  // >
    {0x02, 0x01, 0x51, 0x09, 0x06},  // ?
    {0x32, 0x49, 0x79, 0x41, 0x3E},  // @
    {0x7E, 0x11, 0x11, 0x11, 0x7E},  // A
    {0x7F, 0x49, 0x49, 0x49, 0x36},  // B
    {0x3E, 0x41, 0x41, 0x41, 0x22},  // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C},  // D
    {0x7F, 0x49, 0x49, 0x49, 0x41},  // E
    {0x7F, 0x09, 0x09, 0x01, 0x01},  // F
    {0x3E, 0x41, 0x41, 0x51, 0x32},  // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F},  // H
    {0x00, 0x41, 0x7F, 0x41, 0x00},  // I
    {0x20, 0x40, 0x41, 0x3F, 0x01},  // J
    {0x7F, 0x08, 0x14, 0x22, 0x41},  // K
    {0x7F, 0x40, 0x40, 0x40, 0x40},  // L
    {0x7F, 0x02, 0x04, 0x02, 0x7F},  // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F},  // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E},  // O
    {0x7F, 0x09, 0x09, 0x09, 0x06},  // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E},  // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46},  // R
    {0x46, 0x49, 0x49, 0x49, 0x31},  // S
    {0x01, 0x01, 0x7F, 0x01, 0x01},  // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F},  // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F},  // V
    {0x7F, 0x20, 0x18, 0x20, 0x7F},  // W
    {0x63, 0x14, 0x08, 0x14, 0x63},  // X
    {0x03, 0x04, 0x78, 0x04, 0x03},  // Y
    {0x61, 0x51, 0x49, 0x45, 0x43},  // Z
    {0x00, 0x00, 0x7F, 0x41, 0x41},  // [
    {0x02, 0x04, 0x08, 0x10, 0x20},  // slash
    {0x41, 0x41, 0x7F, 0x00, 0x00},  // ]
    {0x04, 0x02, 0x01, 0x02, 0x04},  // ^
    {0x40, 0x40, 0x40, 0x40, 0x40},  // _
    {0x00, 0x01, 0x02, 0x04, 0x00},  // `
    {0x20, 0x54, 0x54, 0x54, 0x78},  // a
    {0x7F, 0x48, 0x44, 0x44, 0x38},  // b
    {0x38, 0x44, 0x44, 0x44, 0x20},  // c
    {0x38, 0x44, 0x44, 0x48, 0x7F},  // d
    {0x38, 0x54, 0x54, 0x54, 0x18},  // e
    {0x08, 0x7E, 0x09, 0x01, 0x02},  // f
    {0x08, 0x14, 0x54, 0x54, 0x3C},  // g
    {0x7F, 0x08, 0x04, 0x04, 0x78},  // h
    {0x00, 0x44, 0x7D, 0x40, 0x00},  // i
    {0x20, 0x40, 0x44, 0x3D, 0x00},  // j
    {0x00, 0x7F, 0x10, 0x28, 0x44},  // k
    {0x00, 0x41, 0x7F, 0x40, 0x00},  // l
    {0x7C, 0x04, 0x18, 0x04, 0x78},  // m
    {0x7C, 0x08, 0x04, 0x04, 0x78},  // n
    {0x38, 0x44, 0x44, 0x44, 0x38},  // o
    {0x7C, 0x14, 0x14, 0x14, 0x08},  // p
    {0x08, 0x14, 0x14, 0x18, 0x7C},  // q
    {0x7C, 0x08, 0x04, 0x04, 0x08},  // r
    {0x48, 0x54, 0x54, 0x54, 0x20},  // s
    {0x04, 0x3F, 0x44, 0x40, 0x20},  // t
    {0x3C, 0x40, 0x40, 0x20, 0x7C},  // u
    {0x1C, 0x20, 0x40, 0x20, 0x1C},  // v
    {0x3C, 0x40, 0x30, 0x40, 0x3C},  // w
    {0x44, 0x28, 0x10, 0x28, 0x44},  // x
    {0x0C, 0x50, 0x50, 0x50, 0x3C},  // y
    {0x44, 0x64, 0x54, 0x4C, 0x44},  // z
    {0x00, 0x08, 0x36, 0x41, 0x00},  // {
    {0x00, 0x00, 0x7F, 0x00, 0x00},  // |
    {0x00, 0x41, 0x36, 0x08, 0x00},  // }
    {0x08, 0x08, 0x2A, 0x1C, 0x08},  // ->
    {0x08, 0x1C, 0x2A, 0x08, 0x08},  // <-
};

const uint8_t Ssd1309::NULL_ASCII_ICON[5] = {0x7F, 0x7F, 0x7F, 0x7F, 0x7F};

Ssd1309::Ssd1309(uint8_t hwAddr) :
    mAddr(hwAddr),
    mScreen{},
    mIsPowered(false),
    mSendingSequence(SendingSequence::NONE),
    mSentSequence(SendingSequence::NONE),
    mControlQueued(false),
    mControlSeqIdx(NULL_SEQUENCE_IDX),
    mPowerQueued(false),
    mPowerSeqIdx(NULL_SEQUENCE_IDX),
    mScreenQueued(false),
    mHighPriorityScreenQueued(false),
    mScreenSendData(false),
    mScreenSeqRowIdx(NULL_SEQUENCE_IDX),
    mScreenSeqColIdx(NULL_SEQUENCE_IDX),
    mScreenPageSeqIdx(NULL_SEQUENCE_IDX)
{
    queueInitSequence();
}

bool Ssd1309::isPowered()
{
    return mIsPowered;
}

void Ssd1309::queuePower(bool on)
{
    mIsPowered = on;
    mPowerQueued = true;
}

bool Ssd1309::clearScreen()
{
    memset(mScreen, 0, sizeof(mScreen));
    return true;
}

inline void applyScreenDataEightBitRows(
    uint8_t* out,
    uint8_t byte,
    const uint8_t& mask,
    const Ssd1309::MaskType& maskType,
    bool invert)
{
    uint8_t outByte = 0;
    switch(maskType)
    {
        default: // Fall through
        case Ssd1309::MaskType::NONE:
            outByte = (*out & mask) | byte;
            break;

        case Ssd1309::MaskType::AND:
            outByte = *out & (byte | mask);
            break;

        case Ssd1309::MaskType::OR:
            outByte = *out | byte;
            break;

        case Ssd1309::MaskType::XOR:
            outByte = *out ^ byte;
            break;
    }

    if (invert)
    {
        *out = (outByte & mask) | (~outByte & ~mask);
    }
    else
    {
        *out = outByte;
    }
}

inline void applyScreenDataOneBitRowsBottomUp(
    bool set,
    uint8_t* out,
    const uint8_t& outMask,
    const Ssd1309::MaskType& maskType,
    bool invert)
{
    switch(maskType)
    {
        default: // Fall through
        case Ssd1309::MaskType::NONE:
            // Do nothing
            break;

        case Ssd1309::MaskType::AND:
            set &= ((*out & outMask) != 0);
            break;

        case Ssd1309::MaskType::OR:
            set |= ((*out & outMask) != 0);
            break;

        case Ssd1309::MaskType::XOR:
            set ^= ((*out & outMask) != 0);
            break;
    }

    if (set ^ invert)
    {
        *out |= outMask;
    }
    else
    {
        *out &= ~outMask;
    }
}

inline bool computeVisibleArea(
    uint32_t originX,
    uint32_t originY,
    uint32_t width,
    uint32_t height,
    uint32_t& visibleWidth,
    uint32_t& invisibleWidth,
    uint32_t& visibleHeight)
{
    if (originX >= Ssd1309::SCREEN_PIXEL_WIDTH || originY >= Ssd1309::SCREEN_PIXEL_HEIGHT)
    {
        return false;
    }

    visibleWidth = width;
    if (originX + visibleWidth > Ssd1309::SCREEN_PIXEL_WIDTH)
    {
        visibleWidth = Ssd1309::SCREEN_PIXEL_WIDTH - originX;
    }
    invisibleWidth = width - visibleWidth;

    visibleHeight = height;
    if (originY + visibleHeight > Ssd1309::SCREEN_PIXEL_HEIGHT)
    {
        visibleHeight = Ssd1309::SCREEN_PIXEL_HEIGHT - originY;
    }

    return true;
}

bool Ssd1309::setScreenData(
    uint32_t originX,
    uint32_t originY,
    const uint8_t* bytes,
    uint32_t width,
    uint32_t height,
    DataFormat dataFormat,
    MaskType maskType,
    bool invert)
{
    uint32_t visibleWidth = 0;
    uint32_t invisibleWidth = 0;
    uint32_t visibleHeight = 0;

    if (!computeVisibleArea(originX, originY, width, height, visibleWidth, invisibleWidth, visibleHeight))
    {
        return false;
    }

    switch (dataFormat)
    {
    case EIGHT_BIT_ROWS:
    {
        uint8_t lowerBitCount = (originY % 8);
        uint8_t upperBitCount = (8 - lowerBitCount);
        uint8_t upperMaskOut = 0xFF >> upperBitCount;
        uint8_t lowerMaskOut = 0xFF << lowerBitCount;
        uint32_t yIdx = originY / 8;

        for (uint32_t i = 0; i < visibleHeight; i += 8, ++yIdx)
        {
            if (yIdx < (SCREEN_PIXEL_HEIGHT / 8))
            {
                uint8_t byteMask = 0xFF;
                uint8_t upperBitShift = lowerBitCount;
                uint8_t lowerBitShift = upperBitCount;
                if (i + 8 > visibleHeight)
                {
                    // Don't write full 8 bits of bytes in this row
                    uint8_t bitsToWrite = visibleHeight - i;
                    byteMask >>= (8 - bitsToWrite);
                    if (bitsToWrite > upperBitCount)
                    {
                        // right on the boarder -
                        // Write full upper but only partial lower
                        lowerBitCount -= (8 - bitsToWrite);
                        lowerMaskOut = 0xFF << lowerBitCount;
                    }
                    else
                    {
                        // Only write on upper segment
                        upperMaskOut |= (0xFF << (lowerBitCount + bitsToWrite));
                        upperBitCount = bitsToWrite;
                        lowerBitCount = 0;
                        lowerMaskOut = 0xFF;
                    }
                }

                uint8_t* yTopOut = mScreen[yIdx];
                uint8_t* yBottomOut = nullptr;
                if (lowerMaskOut != 0xFF && (yIdx + 1) < (SCREEN_PIXEL_HEIGHT / 8))
                {
                    yBottomOut = mScreen[yIdx + 1];
                }
                uint8_t* xyTopOut = &yTopOut[originX];
                uint8_t* xyBottomOut = nullptr;
                if (yBottomOut != nullptr)
                {
                    xyBottomOut = &yBottomOut[originX];
                }

                for (uint32_t j = 0; j < visibleWidth; ++j, ++xyTopOut, ++bytes)
                {
                    uint8_t byte = *bytes & byteMask;

                    applyScreenDataEightBitRows(xyTopOut, byte << upperBitShift, upperMaskOut, maskType, invert);

                    if (xyBottomOut != nullptr)
                    {
                        applyScreenDataEightBitRows(xyBottomOut, byte >> lowerBitShift, lowerMaskOut, maskType, invert);
                        ++xyBottomOut;
                    }
                }
            }
            else
            {
                bytes += visibleWidth;
            }
            bytes += invisibleWidth;
        }
    }
    break;

    case ONE_BIT_ROWS_BOTTOM_UP:
    {
        uint8_t yRemainder = ((originY + visibleHeight) % 8);
        uint8_t outMask = 0x80 >> yRemainder;
        uint8_t inMask = 0x80;
        uint32_t yIdx = (originY + visibleHeight - 7) / 8; // This is probably incorrect
        for (uint32_t i = 0; i < visibleHeight; ++i)
        {
            uint8_t* out = &mScreen[yIdx][originX + visibleWidth - 1];

            for (uint32_t j = 0; j < invisibleWidth % 8; ++j)
            {
                inMask >>= 1;
                if (inMask == 0)
                {
                    inMask = 0x80;
                    ++bytes;
                }
            }

            for (uint32_t j = 0; j < visibleWidth; ++j, --out)
            {
                bool set = *bytes & inMask;
                applyScreenDataOneBitRowsBottomUp(*bytes & inMask, out, outMask, maskType, invert);

                inMask >>= 1;
                if (inMask == 0)
                {
                    inMask = 0x80;
                    ++bytes;
                }
            }

            outMask >>= 1;
            if (outMask == 0)
            {
                outMask = 0x80;
                --yIdx;
            }
        }
    }
    break;

    default: return false;
    }

    return true;
}

bool Ssd1309::setScreenData(
    uint32_t originX,
    uint32_t originY,
    const Icon& icon,
    MaskType maskType,
    bool invert)
{
    return setScreenData(originX, originY, icon.image, icon.width, icon.height, icon.dataFormat, maskType, invert);
}

bool Ssd1309::clearScreenData(uint32_t originX, uint32_t originY, uint32_t width, uint32_t height, bool clr)
{
    uint32_t visibleWidth = 0;
    uint32_t invisibleWidth = 0;
    uint32_t visibleHeight = 0;

    if (!computeVisibleArea(originX, originY, width, height, visibleWidth, invisibleWidth, visibleHeight))
    {
        return false;
    }

    uint32_t numBits = 8;
    uint32_t yIdx = originY / 8;

    for (uint32_t i = 0; i < visibleHeight; i += numBits, ++yIdx)
    {
        numBits = 8;
        uint8_t byteMask = 0;

        if (i == 0)
        {
            // Don't write to lower bits at top
            numBits = 8 - (originY % 8);
            byteMask = 0xFF >> numBits;
        }

        if (i + numBits > visibleHeight)
        {
            // Don't write to upper bits at bottom
            uint8_t bitsToWrite = visibleHeight - i;
            byteMask ^= ~(0xFF >> (numBits - bitsToWrite));
            numBits = bitsToWrite;
        }

        uint8_t* out = &mScreen[yIdx][originX];

        for (uint32_t j = 0; j < visibleWidth; ++j, ++out)
        {
            if (clr)
            {
                *out &= byteMask;
            }
            else
            {
                *out |= ~byteMask;
            }
        }
    }

    return true;
}

bool Ssd1309::clearScreenData(uint32_t originX, uint32_t originY, const Icon& icon)
{
    return clearScreenData(originX, originY, icon.width, icon.height);
}

void Ssd1309::invertScreenData(uint32_t originX, uint32_t originY, uint32_t width, uint32_t height)
{
    uint32_t visibleWidth = 0;
    uint32_t invisibleWidth = 0;
    uint32_t visibleHeight = 0;

    if (!computeVisibleArea(originX, originY, width, height, visibleWidth, invisibleWidth, visibleHeight))
    {
        return;
    }

    uint8_t remainder = (originY % 8);
    uint8_t numBits = 8 - remainder;
    uint8_t mask = 0xFF << remainder;
    uint32_t yIdx = originY / 8;
    uint32_t i = 0;
    while (numBits > 0)
    {
        uint8_t* out = &mScreen[yIdx++][originX];

        if (numBits < 8)
        {
            for (uint32_t j = 0; j < visibleWidth; ++j, ++out)
            {
                *out = ((*out) & ~mask) | (~(*out) & mask);
            }
        }
        else
        {
            for (uint32_t j = 0; j < visibleWidth; ++j, ++out)
            {
                *out = ~(*out);
            }
        }

        i += numBits;
        uint32_t bitsLeft = visibleHeight - i;
        if (bitsLeft >= 8)
        {
            numBits = 8;
            mask = 0xFF;
        }
        else
        {
            numBits = bitsLeft;
            mask = 0xFF >> (8 - bitsLeft);
        }
    }
}

bool Ssd1309::setScreenText(
    uint32_t originX,
    uint32_t originY,
    uint32_t sizeMultiplier,
    const char* text,
    MaskType maskType,
    bool invert)
{
    if (originX >= SCREEN_PIXEL_WIDTH || originY >= SCREEN_PIXEL_HEIGHT)
    {
        return false;
    }

    sizeMultiplier = sizeMultiplier > 8 ? 8 : sizeMultiplier;
    sizeMultiplier = sizeMultiplier < 1 ? 1 : sizeMultiplier;

    uint32_t len = strlen(text);
    const uint32_t width = 6 * sizeMultiplier * len;
    const uint32_t heightBytes = sizeMultiplier;
    uint8_t buffer[heightBytes][width] = {};
    uint32_t wIdx = 0;

    // Each character in text string
    for (uint32_t i = 0; i < len; ++i, ++text)
    {
        char letter = *text;
        const uint8_t *sequence = nullptr;
        if (letter < ' ' || letter > 127)
        {
            sequence = NULL_ASCII_ICON;
        }
        else
        {
            sequence = ASCII_ICONS[letter - ' '];
        }

        if (sizeMultiplier <= 1)
        {
            // Simple copy
            memcpy(&buffer[0][wIdx], sequence, 5);
            wIdx += 5;
        }
        else
        {
            // Resize sequence into buffer

            // Each byte in character sequence
            for (uint32_t n = 0; n < 5; ++n, ++sequence, wIdx += sizeMultiplier)
            {
                uint32_t hIdx = 0;
                uint8_t outMask = 0x01;
                // Each bit in the current sequence byte
                for (uint8_t inMask = 0x01; inMask > 0; inMask <<= 1)
                {
                    // Height resizing
                    for (uint32_t j = 0; j < sizeMultiplier; ++j)
                    {
                        // Width resizing
                        for (uint32_t k = 0; k < sizeMultiplier; ++k)
                        {
                            if (*sequence & inMask)
                            {
                                buffer[hIdx][wIdx + k] |= outMask;
                            }
                        }
                        outMask <<= 1;
                        if (outMask == 0)
                        {
                            ++hIdx;
                            outMask = 0x01;
                        }
                    }
                }
            }
        }

        // Empty column
        wIdx += sizeMultiplier;
    }

    setScreenData(
        originX, originY, &buffer[0][0], width, heightBytes * 8,
        Ssd1309::DataFormat::EIGHT_BIT_ROWS, maskType, invert);

    return true;
}

void Ssd1309::queueScreenWrite()
{
    mScreenQueued = true;
}

void Ssd1309::queueInitSequence()
{
    mControlQueued = true;
    queuePower(true);
    mHighPriorityScreenQueued = true;
}


//
// I2cDevice interfaces
//

uint8_t Ssd1309::getAddress()
{
    return mAddr;
}

bool Ssd1309::nextByteOut(uint8_t &byte, bool& sendStop)
{
    assert(mSendingSequence != SendingSequence::NONE);

    bool moreDataAvailable = false;

    switch (mSendingSequence)
    {
        case SendingSequence::CONTROL:
        {
            byte = INIT_CONTROL_SEQUENCE[mControlSeqIdx++];
            moreDataAvailable = (mControlSeqIdx < sizeof(INIT_CONTROL_SEQUENCE));
        }
        break;

        case SendingSequence::POWER:
        {
            switch (mPowerSeqIdx++)
            {
                case 0:
                    byte = CONTROL_BYTE;
                    moreDataAvailable = true;
                    break;

                case 1:
                    byte = (mIsPowered ? CONTROL_POWER_ON : CONTROL_POWER_OFF);
                    moreDataAvailable = false;
                    break;

                default:
                    // Shouldn't reach here
                    byte = 0;
                    moreDataAvailable = false;
                    break;
            }
        }
        break;

        case SendingSequence::SCREEN:
        {
            if (mScreenPageSeqIdx < sizeof(PAGE_CONTROL_SEQUENCE))
            {
                byte = PAGE_CONTROL_SEQUENCE[mScreenPageSeqIdx];
                if (mScreenPageSeqIdx == 1)
                {
                    // Update byte with which page we are writing
                    uint8_t page = mScreenSeqRowIdx;
                    byte |= (page & 0x0F);
                }
                if (++mScreenPageSeqIdx >= sizeof(PAGE_CONTROL_SEQUENCE))
                {
                    // End of page sequence
                    sendStop = true;
                    // Next byte should be start of data sequence
                    mScreenSendData = true;
                }
                moreDataAvailable = true;
            }
            else if (mScreenSendData)
            {
                byte = DATA_BYTE;
                mScreenSendData = false;
                moreDataAvailable = true;
            }
            else
            {
                assert(mScreenSeqRowIdx < (SCREEN_PIXEL_HEIGHT / 8));
                assert(mScreenSeqColIdx < SCREEN_PIXEL_WIDTH);
                // Send screen byte
                byte = mScreen[mScreenSeqRowIdx][mScreenSeqColIdx];
                if (++mScreenSeqColIdx >= SCREEN_PIXEL_WIDTH)
                {
                    // End of this row data sequence
                    sendStop = true;
                    // Advance to next row
                    mScreenSeqColIdx = 0;
                    if (++mScreenSeqRowIdx < (SCREEN_PIXEL_HEIGHT / 8))
                    {
                        // Next byte should be page control sequence
                        mScreenPageSeqIdx = 0;
                        mScreenSendData = true;
                        moreDataAvailable = true;
                    }
                    // else: no more data available
                }
                else
                {
                    moreDataAvailable = true;
                }
            }
        }
        break;

        default:
        {
            assert(false);
        }
        break;
    }

    if (!moreDataAvailable)
    {
        mSentSequence = mSendingSequence;
        mSendingSequence = SendingSequence::NONE;
    }

    return moreDataAvailable;
}

bool Ssd1309::isOutAvailable()
{
    if (mSendingSequence == SendingSequence::NONE)
    {
        // Check for anything that is queued
        if (mControlQueued)
        {
            mControlQueued = false;
            mControlSeqIdx = 0;
            mSendingSequence = SendingSequence::CONTROL;
            return true;
        }
        else if (mPowerQueued && !mHighPriorityScreenQueued)
        {
            mPowerQueued = false;
            mPowerSeqIdx = 0;
            mSendingSequence = SendingSequence::POWER;
            return true;
        }
        else if (mScreenQueued || mHighPriorityScreenQueued)
        {
            mScreenQueued = false;
            mHighPriorityScreenQueued = false;
            mScreenPageSeqIdx = 0;
            mScreenSeqRowIdx = 0;
            mScreenSeqColIdx = 0;
            mScreenSendData = true;
            mSendingSequence = SendingSequence::SCREEN;
            return true;
        }
    }
    else
    {
        // Shouldn't reach here, but handle it
        return true;
    }

    return false;
}

void Ssd1309::writeFailed(uint32_t abortReason)
{
    SendingSequence testSeq = (mSendingSequence == SendingSequence::NONE) ? mSentSequence : mSendingSequence;
    switch (testSeq)
    {
        case SendingSequence::CONTROL:
            // Retry
            mControlQueued = true;
            break;

        case SendingSequence::POWER:
            // Retry
            mPowerQueued = true;
            break;

        case SendingSequence::SCREEN:
            // Retry
            mScreenQueued = true;
            break;

        default:
            // Do nothing
            break;
    }
    // No longer sending anything
    mSendingSequence = SendingSequence::NONE;
}

void Ssd1309::nextByteIn(uint8_t byte)
{
    // Do nothing
}

uint32_t Ssd1309::isInExpected()
{
    // No read operation done in this class
    return 0;
}

void Ssd1309::readFailed(uint32_t abortReason)
{
    // Do nothing
}
