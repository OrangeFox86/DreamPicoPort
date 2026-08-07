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

#include "MockMutex.hpp"

#include "hal/Usb/WebUsbProcessorBase.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <cstdint>
#include <vector>
#include <list>

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgReferee;
using ::testing::DoAll;

class TestWebUsbProcessor : public WebUsbProcessorBase
{
public:
    TestWebUsbProcessor(uint8_t itf) : WebUsbProcessorBase(itf) {}

    MOCK_METHOD(
        void,
        processPkt,
        (const std::string& address, const uint8_t cmd, const uint8_t* payload, uint16_t payloadLen),
        (override)
    );

    MOCK_METHOD(
        std::uint32_t,
        vendorWrite,
        (const void* buffer, std::uint32_t bufsize, bool flush, bool task),
        (override)
    );
};

class WebUsbProcessorTest : public ::testing::Test
{
protected:
    TestWebUsbProcessor mTestWebUsbProcessor{0};
    MockMutex mMockMutex;

    void SetUp() override
    {
        mTestWebUsbProcessor.init(&mMockMutex);
        EXPECT_CALL(mMockMutex, lock).Times(::testing::AtLeast(0));
        EXPECT_CALL(mMockMutex, unlock).Times(::testing::AtLeast(0));
        EXPECT_CALL(mMockMutex, tryLock).Times(::testing::AtLeast(0)).WillRepeatedly(Return(1));
    }
};

TEST_F(WebUsbProcessorTest, single_packet_added__process__success)
{
    // Arrange
    std::vector<std::uint8_t> data = {
        0xDB, 0x8B, 0xAF, 0xD5, // Magic
        0x00, 0x08, // Size
        0xFF, 0xF7, // Inverse Size
        0x5A, // Address
        0xA5, // Command
        0x01, 0x02, 0x10, 0x20, // Payload
        0xCE, 0x70 // CRC
    };
    mTestWebUsbProcessor.addData(data.data(), data.size());

    std::vector<std::uint8_t> receivedPayload;
    EXPECT_CALL(
        mTestWebUsbProcessor, processPkt(std::string("\x5a", 1), 0xA5, _, _)
    ).Times(1).WillOnce(::testing::Invoke(
        [&receivedPayload]
        (const std::string& address, const uint8_t cmd, const uint8_t* payload, uint16_t payloadLen)
        {
            receivedPayload.assign(payload, payload + payloadLen);
        }
    ));

    // Act
    mTestWebUsbProcessor.process();

    // Assert
    const std::vector<std::uint8_t> expectedPayload{0x01, 0x02, 0x10, 0x20};
    EXPECT_EQ(receivedPayload, expectedPayload);
}

TEST_F(WebUsbProcessorTest, split_packets_added__process__success)
{
    // Arrange
    EXPECT_CALL(mTestWebUsbProcessor, processPkt).Times(0);

    std::vector<std::uint8_t> data1 = {
        0xDB, 0x8B, 0xAF, 0xD5, // Magic
        0x00, 0x08, // Size
    };
    std::vector<std::uint8_t> data2 = {
        0xFF, 0xF7, // Inverse Size
        0x5A, // Address
    };
    std::vector<std::uint8_t> data3 = {
        0xA5, // Command
        0x01, 0x02, 0x10, 0x20, // Payload
        0xCE, 0x70 // CRC
    };
    mTestWebUsbProcessor.addData(data1.data(), data1.size());
    mTestWebUsbProcessor.process();
    mTestWebUsbProcessor.addData(data2.data(), data2.size());
    mTestWebUsbProcessor.process();
    mTestWebUsbProcessor.addData(data3.data(), data3.size());

    std::vector<std::uint8_t> receivedPayload;
    EXPECT_CALL(
        mTestWebUsbProcessor, processPkt(std::string("\x5a", 1), 0xA5, _, _)
    ).Times(1).WillOnce(::testing::Invoke(
        [&receivedPayload]
        (const std::string& address, const uint8_t cmd, const uint8_t* payload, uint16_t payloadLen)
        {
            receivedPayload.assign(payload, payload + payloadLen);
        }
    ));

    // Act
    mTestWebUsbProcessor.process();

    // Assert
    const std::vector<std::uint8_t> expectedPayload{0x01, 0x02, 0x10, 0x20};
    EXPECT_EQ(receivedPayload, expectedPayload);
}

TEST_F(WebUsbProcessorTest, multi_packet_added__process__success)
{
    // Arrange
    std::vector<std::uint8_t> data = {
        // Packet 1
        0xDB, 0x8B, 0xAF, 0xD5, // Magic
        0x00, 0x08, // Size
        0xFF, 0xF7, // Inverse Size
        0x5A, // Address
        0xA5, // Command
        0x01, 0x02, 0x10, 0x20, // Payload
        0xCE, 0x70, // CRC

        // Packet 2
        0xDB, 0x8B, 0xAF, 0xD5, // Magic
        0x00, 0x0A, // Size
        0xFF, 0xF5, // Inverse Size
        0xCA, 0x00, // Address
        0x81, // Command
        0x10, 0x20, 0x01, 0x02, 0x03, // Payload
        0x68, 0xB6, // CRC

        // Packet 3
        0xDB, 0x8B, 0xAF, 0xD5, // Magic
        0x00, 0x0C, // Size
        0xFF, 0xF3, // Inverse Size
        0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, // Address
        0xF0, // Command
         // Payload
        0x08, 0xD2, // CRC
    };
    mTestWebUsbProcessor.addData(data.data(), data.size());

    std::list<std::vector<std::uint8_t>> receivedPayloads;
    auto mockCb =
        [&receivedPayloads]
        (const std::string& address, const uint8_t cmd, const uint8_t* payload, uint16_t payloadLen)
        {
            receivedPayloads.push_back(std::vector<std::uint8_t>(payload, payload + payloadLen));
        };

    EXPECT_CALL(
        mTestWebUsbProcessor, processPkt(std::string("\x5a", 1), 0xA5, _, _)
    ).Times(1).WillOnce(::testing::Invoke(mockCb));
    EXPECT_CALL(
        mTestWebUsbProcessor, processPkt(std::string("\xca\x00", 2), 0x81, _, _)
    ).Times(1).WillOnce(::testing::Invoke(mockCb));
    EXPECT_CALL(
        mTestWebUsbProcessor, processPkt(std::string("\x80\x80\x80\x80\x80\x80\x80\x80\x80", 9), 0xF0, _, _)
    ).Times(1).WillOnce(::testing::Invoke(mockCb));

    // Act
    mTestWebUsbProcessor.process();

    // Assert
    const std::list<std::vector<std::uint8_t>> expectedPayloads{{0x01, 0x02, 0x10, 0x20}, {0x10, 0x20, 0x01, 0x02, 0x03}, {}};
    EXPECT_EQ(receivedPayloads, expectedPayloads);
}

TEST_F(WebUsbProcessorTest, multi_packet_added_crc_invalid__process__one_ignored)
{
    // Arrange
    std::vector<std::uint8_t> data = {
        // Packet 1
        0xDB, 0x8B, 0xAF, 0xD5, // Magic
        0x00, 0x08, // Size
        0xFF, 0xF7, // Inverse Size
        0x5A, // Address
        0xA5, // Command
        0x01, 0x02, 0x10, 0x20, // Payload
        0xCE, 0x70, // CRC

        // Packet 2
        0xDB, 0x8B, 0xAF, 0xD5, // Magic
        0x00, 0x0A, // Size
        0xFF, 0xF5, // Inverse Size
        0xCA, 0x00, // Address
        0x81, // Command
        0x10, 0x20, 0x01, 0x02, 0x03, // Payload
        0, 0, // CRC (invalid)

        // Packet 3
        0xDB, 0x8B, 0xAF, 0xD5, // Magic
        0x00, 0x0C, // Size
        0xFF, 0xF3, // Inverse Size
        0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, // Address
        0xF0, // Command
         // Payload
        0x08, 0xD2, // CRC
    };
    mTestWebUsbProcessor.addData(data.data(), data.size());

    std::list<std::vector<std::uint8_t>> receivedPayloads;
    auto mockCb =
        [&receivedPayloads]
        (const std::string& address, const uint8_t cmd, const uint8_t* payload, uint16_t payloadLen)
        {
            receivedPayloads.push_back(std::vector<std::uint8_t>(payload, payload + payloadLen));
        };

    EXPECT_CALL(
        mTestWebUsbProcessor, processPkt(std::string("\x5a", 1), 0xA5, _, _)
    ).Times(1).WillOnce(::testing::Invoke(mockCb));
    EXPECT_CALL(
        mTestWebUsbProcessor, processPkt(std::string("\x80\x80\x80\x80\x80\x80\x80\x80\x80", 9), 0xF0, _, _)
    ).Times(1).WillOnce(::testing::Invoke(mockCb));

    // Act
    mTestWebUsbProcessor.process();

    // Assert
    const std::list<std::vector<std::uint8_t>> expectedPayloads{{0x01, 0x02, 0x10, 0x20}, {}};
    EXPECT_EQ(receivedPayloads, expectedPayloads);
}
