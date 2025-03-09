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

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <memory>

#include <SerialStreamParser.hpp>
#include "MockCommandParser.hpp"
#include "MockMutex.hpp"

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgReferee;
using ::testing::DoAll;
using ::testing::StrEq;
using ::testing::AnyNumber;
using ::testing::Invoke;

class SerialStreamParserTest : public ::testing::Test
{
protected:
    std::unique_ptr<SerialStreamParser> serialStreamParser;
    std::shared_ptr<MockCommandParser> mockCmdParser;
    MockMutex mockMutex;

    SerialStreamParserTest() = default;

    void SetUp() override
    {
        EXPECT_CALL(mockMutex, lock).Times(AnyNumber());
        EXPECT_CALL(mockMutex, tryLock).Times(AnyNumber()).WillRepeatedly(Return(1));
        EXPECT_CALL(mockMutex, unlock).Times(AnyNumber());
        mockCmdParser = std::make_shared<MockCommandParser>();
        serialStreamParser = std::make_unique<SerialStreamParser>(mockMutex, 'h');
        EXPECT_CALL(*mockCmdParser, getCommandChars).WillRepeatedly(Return("XYZ"));
        serialStreamParser->addCommandParser(mockCmdParser);
    }
};

TEST_F(SerialStreamParserTest, partial_command__no_action)
{
    // Arrange
    const std::string chars("XThis is a partial command without newline");
    serialStreamParser->addChars(chars.c_str(), chars.size());

    std::list<std::string> receivedStrings;
    EXPECT_CALL(*mockCmdParser, submit).WillRepeatedly(Invoke(
        [&receivedStrings]
        (const char* chars, uint32_t len) -> void
        {
            receivedStrings.push_back(std::string(chars, len));
        }
    ));

    // Act
    serialStreamParser->process();

    // Assert
    EXPECT_TRUE(receivedStrings.empty());
    EXPECT_EQ(serialStreamParser->numBufferedChars(), chars.size());
    EXPECT_EQ(serialStreamParser->numBufferedCmds(), 0);
}

TEST_F(SerialStreamParserTest, binary_partial_command__no_action)
{
    // Arrange
    const std::string chars =
        std::string("X") +
        TtyParser::BINARY_START_CHAR +
        static_cast<char>(0) +
        static_cast<char>(100) +
        std::string("This is binary data which isn't complete");
    serialStreamParser->addChars(chars.c_str(), chars.size());

    std::list<std::string> receivedStrings;
    EXPECT_CALL(*mockCmdParser, submit).WillRepeatedly(Invoke(
        [&receivedStrings]
        (const char* chars, uint32_t len) -> void
        {
            receivedStrings.push_back(std::string(chars, len));
        }
    ));

    // Act
    serialStreamParser->process();

    // Assert
    EXPECT_TRUE(receivedStrings.empty());
    EXPECT_EQ(serialStreamParser->numBufferedChars(), chars.size());
    EXPECT_EQ(serialStreamParser->numBufferedCmds(), 0);
}

TEST_F(SerialStreamParserTest, binary_complete_without_newline__no_action)
{
    // Arrange
    const std::string chars =
        std::string("X") +
        TtyParser::BINARY_START_CHAR +
        static_cast<char>(0) +
        static_cast<char>(100) +
        std::string(100, '\n');
    serialStreamParser->addChars(chars.c_str(), chars.size());

    std::list<std::string> receivedStrings;
    EXPECT_CALL(*mockCmdParser, submit).WillRepeatedly(Invoke(
        [&receivedStrings]
        (const char* chars, uint32_t len) -> void
        {
            receivedStrings.push_back(std::string(chars, len));
        }
    ));

    // Act
    serialStreamParser->process();

    // Assert
    EXPECT_TRUE(receivedStrings.empty());
    EXPECT_EQ(serialStreamParser->numBufferedChars(), chars.size());
    EXPECT_EQ(serialStreamParser->numBufferedCmds(), 0);
}

TEST_F(SerialStreamParserTest, full_command__submitted)
{
    // Arrange
    const std::string chars("XThis is a full command\n");
    serialStreamParser->addChars(chars.c_str(), chars.size());

    std::list<std::string> receivedStrings;
    EXPECT_CALL(*mockCmdParser, submit).WillRepeatedly(Invoke(
        [&receivedStrings]
        (const char* chars, uint32_t len) -> void
        {
            receivedStrings.push_back(std::string(chars, len));
        }
    ));

    // Act
    serialStreamParser->process();

    // Assert
    ASSERT_EQ(receivedStrings.size(), 1);
    EXPECT_EQ(*receivedStrings.begin(), std::string("XThis is a full command"));
    EXPECT_EQ(serialStreamParser->numBufferedChars(), 0);
    EXPECT_EQ(serialStreamParser->numBufferedCmds(), 0);
}

TEST_F(SerialStreamParserTest, binary_full_command__submitted)
{
    // Arrange
    const std::string chars =
        std::string("X") +
        TtyParser::BINARY_START_CHAR +
        static_cast<char>(0) +
        static_cast<char>(100) +
        std::string(100, '\n') +
        '\n';
    serialStreamParser->addChars(chars.c_str(), chars.size());

    std::string received;
    EXPECT_CALL(*mockCmdParser, submit).Times(1).WillOnce(Invoke(
        [&received]
        (const char* chars, uint32_t len) -> void
        {
            received = std::string(chars, len);
        }
    ));

    // Act
    serialStreamParser->process();

    // Assert
    EXPECT_EQ(received, chars.substr(0, chars.size() - 1));
    EXPECT_EQ(serialStreamParser->numBufferedChars(), 0);
    EXPECT_EQ(serialStreamParser->numBufferedCmds(), 0);
}

TEST_F(SerialStreamParserTest, full_command_over_two_messages__submitted)
{
    // Arrange
    const std::string chars1("YThis is a fu");
    serialStreamParser->addChars(chars1.c_str(), chars1.size());
    const std::string chars2("ll command\n");
    serialStreamParser->addChars(chars2.c_str(), chars2.size());

    std::string received;
    EXPECT_CALL(*mockCmdParser, submit).Times(1).WillOnce(Invoke(
        [&received]
        (const char* chars, uint32_t len) -> void
        {
            received = std::string(chars, len);
        }
    ));

    // Act
    serialStreamParser->process();

    // Assert
    EXPECT_EQ(received, std::string("YThis is a full command"));
    EXPECT_EQ(serialStreamParser->numBufferedChars(), 0);
    EXPECT_EQ(serialStreamParser->numBufferedCmds(), 0);
}

TEST_F(SerialStreamParserTest, binary_full_command_over_two_messages__submitted)
{
    // Arrange
    const std::string chars1 =
        std::string("Y") +
        TtyParser::BINARY_START_CHAR +
        static_cast<char>(0) +
        static_cast<char>(100) +
        std::string(50, '\n');
    serialStreamParser->addChars(chars1.c_str(), chars1.size());
    const std::string chars2 =
        std::string(50, '\n') +
        '\n';
    serialStreamParser->addChars(chars2.c_str(), chars2.size());

    std::string received;
    EXPECT_CALL(*mockCmdParser, submit).Times(1).WillOnce(Invoke(
        [&received]
        (const char* chars, uint32_t len) -> void
        {
            received = std::string(chars, len);
        }
    ));

    // Act
    serialStreamParser->process();

    // Assert
    std::string expected = chars1 + chars2;
    expected = expected.substr(0, expected.size() - 1);
    EXPECT_EQ(received, expected);
    EXPECT_EQ(serialStreamParser->numBufferedChars(), 0);
    EXPECT_EQ(serialStreamParser->numBufferedCmds(), 0);
}

TEST_F(SerialStreamParserTest, multiple_command__overflow)
{
    // Arrange
    const std::string chars1("ZThis is a full command\n");
    const std::size_t count = 2048 / chars1.size();
    for (std::size_t i = 0; i < count; ++i)
    {
        serialStreamParser->addChars(chars1.c_str(), chars1.size());
    }
    const std::string chars2("XThis command will overflow the parser\n");
    serialStreamParser->addChars(chars2.c_str(), chars2.size());

    std::list<std::string> receivedStrings;
    EXPECT_CALL(*mockCmdParser, submit).WillRepeatedly(Invoke(
        [&receivedStrings]
        (const char* chars, uint32_t len) -> void
        {
            std::string s(chars, len);
            receivedStrings.push_back(std::move(s));
        }
    ));

    // Act
    for (std::size_t i = 0; i < count; ++i)
    {
        serialStreamParser->process();
    }
    serialStreamParser->process();

    // Assert
    ASSERT_EQ(receivedStrings.size(), count);
    const std::string expected("ZThis is a full command");
    for (const std::string& s : receivedStrings)
    {
        EXPECT_EQ(s, expected);
    }
    EXPECT_EQ(serialStreamParser->numBufferedChars(), 0);
    EXPECT_EQ(serialStreamParser->numBufferedCmds(), 0);
}

TEST_F(SerialStreamParserTest, binary_multiple_command__overflow)
{
    // Arrange
    const std::string chars1 =
        std::string("X") +
        TtyParser::BINARY_START_CHAR +
        static_cast<char>(0) +
        static_cast<char>(100) +
        std::string(100, '\n') +
        '\n';
    const std::size_t count = 2048 / chars1.size();
    for (std::size_t i = 0; i < count; ++i)
    {
        serialStreamParser->addChars(chars1.c_str(), chars1.size());
    }
    const std::string chars2 =
        std::string("Y") +
        TtyParser::BINARY_START_CHAR +
        static_cast<char>(0) +
        static_cast<char>(100) +
        std::string(100, '\n') +
        '\n';
    serialStreamParser->addChars(chars2.c_str(), chars2.size());

    std::list<std::string> receivedStrings;
    EXPECT_CALL(*mockCmdParser, submit).WillRepeatedly(Invoke(
        [&receivedStrings]
        (const char* chars, uint32_t len) -> void
        {
            receivedStrings.push_back(std::string(chars, len));
        }
    ));

    // Act
    for (std::size_t i = 0; i < count; ++i)
    {
        serialStreamParser->process();
    }
    serialStreamParser->process();

    // Assert
    ASSERT_EQ(receivedStrings.size(), count);
    const std::string expected = chars1.substr(0, chars1.size() - 1);
    for (const std::string& s : receivedStrings)
    {
        EXPECT_EQ(s, expected);
    }
    EXPECT_EQ(serialStreamParser->numBufferedChars(), 0);
    EXPECT_EQ(serialStreamParser->numBufferedCmds(), 0);
}

TEST_F(SerialStreamParserTest, single_command__overflow)
{
    // Arrange
    const std::string chars = std::string(2049, 'X') + std::string("\n");
    serialStreamParser->addChars(chars.c_str(), chars.size());

    std::list<std::string> receivedStrings;
    EXPECT_CALL(*mockCmdParser, submit).WillRepeatedly(Invoke(
        [&receivedStrings]
        (const char* chars, uint32_t len) -> void
        {
            receivedStrings.push_back(std::string(chars, len));
        }
    ));

    // Act
    serialStreamParser->process();

    // Assert
    EXPECT_TRUE(receivedStrings.empty());
    EXPECT_EQ(serialStreamParser->numBufferedChars(), 0);
    EXPECT_EQ(serialStreamParser->numBufferedCmds(), 0);
}

TEST_F(SerialStreamParserTest, binary_single_command__overflow)
{
    // Arrange
    const std::string chars =
        std::string(2000, 'X') +
        TtyParser::BINARY_START_CHAR +
        static_cast<char>(0) +
        static_cast<char>(100) +
        std::string(100, '\n') +
        '\n';
    serialStreamParser->addChars(chars.c_str(), chars.size());

    std::list<std::string> receivedStrings;
    EXPECT_CALL(*mockCmdParser, submit).WillRepeatedly(Invoke(
        [&receivedStrings]
        (const char* chars, uint32_t len) -> void
        {
            receivedStrings.push_back(std::string(chars, len));
        }
    ));

    // Act
    serialStreamParser->process();

    // Assert
    EXPECT_TRUE(receivedStrings.empty());
    EXPECT_EQ(serialStreamParser->numBufferedChars(), 0);
    EXPECT_EQ(serialStreamParser->numBufferedCmds(), 0);
}

TEST_F(SerialStreamParserTest, full_command_with_backspaces__submitted)
{
    // Arrange
    const std::string chars("XThis is a fullly\x08\x08 command\n");
    serialStreamParser->addChars(chars.c_str(), chars.size());

    std::string received;
    EXPECT_CALL(*mockCmdParser, submit).Times(1).WillOnce(Invoke(
        [&received]
        (const char* chars, uint32_t len) -> void
        {
            received = std::string(chars, len);
        }
    ));

    // Act
    serialStreamParser->process();

    // Assert
    EXPECT_EQ(received, std::string("XThis is a full command"));
    EXPECT_EQ(serialStreamParser->numBufferedChars(), 0);
    EXPECT_EQ(serialStreamParser->numBufferedCmds(), 0);
}

TEST_F(SerialStreamParserTest, help)
{
    // Arrange
    const std::string chars("h\n");
    serialStreamParser->addChars(chars.c_str(), chars.size());

    // Assert
    EXPECT_CALL(*mockCmdParser, printHelp).Times(1);

    // Act
    serialStreamParser->process();
}

TEST_F(SerialStreamParserTest, invalid_command)
{
    // Arrange
    const std::string chars("QThis command won't be processed\n");
    serialStreamParser->addChars(chars.c_str(), chars.size());

    std::list<std::string> receivedStrings;
    EXPECT_CALL(*mockCmdParser, submit).WillRepeatedly(Invoke(
        [&receivedStrings]
        (const char* chars, uint32_t len) -> void
        {
            receivedStrings.push_back(std::string(chars, len));
        }
    ));

    // Act
    serialStreamParser->process();

    // Assert
    EXPECT_TRUE(receivedStrings.empty());
    EXPECT_EQ(serialStreamParser->numBufferedChars(), 0);
    EXPECT_EQ(serialStreamParser->numBufferedCmds(), 0);
}
