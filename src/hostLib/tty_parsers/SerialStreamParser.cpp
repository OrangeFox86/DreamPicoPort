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

#include "SerialStreamParser.hpp"
#include "hal/System/LockGuard.hpp"

#include <limits>
#include <string.h>
#include <algorithm>
#include <stdio.h>

const char* SerialStreamParser::WHITESPACE_CHARS = "\r\n\t ";
const char* SerialStreamParser::INPUT_EOL_CHARS = "\r\n";
const char* SerialStreamParser::BACKSPACE_CHARS = "\x08\x7F";

SerialStreamParser::SerialStreamParser(MutexInterface& m, char helpChar) :
    mParserRx(),
    mEndMarkers(),
    mLastIsEol(false),
    mParserMutex(m),
    mHelpChar(helpChar),
    mParsers(),
    mOverflowDetected(false),
    mNumBinaryParsed(-1),
    mStoredBinarySize(0),
    mNumBinaryLeft(0)
{}

void SerialStreamParser::addCommandParser(std::shared_ptr<CommandParser> parser)
{
    mParsers.push_back(parser);
}

void SerialStreamParser::addChars(const char* chars, uint32_t len)
{
    // Entire function is locked
    LockGuard lockGuard(mParserMutex);

    // Reserve space for new characters
    uint32_t newCapacity = mParserRx.size() + len;
    if (newCapacity > MAX_QUEUE_SIZE)
    {
        newCapacity = MAX_QUEUE_SIZE;
    }
    mParserRx.reserve(newCapacity);

    for (uint32_t i = 0; i < len; ++i, ++chars)
    {
        // Flag overflow - next command will be ignored
        if (mParserRx.size() >= MAX_QUEUE_SIZE && *chars != 0x08)
        {
            mOverflowDetected = true;
        }

        if (mNumBinaryParsed >= 0)
        {
            ++mNumBinaryParsed;
            --mNumBinaryLeft;

            if (mNumBinaryParsed == 1)
            {
                mStoredBinarySize = (*chars) << 8;
            }
            else if (mNumBinaryParsed == 2)
            {
                mStoredBinarySize |= (*chars);
                mNumBinaryLeft = mStoredBinarySize;
            }

            if (mNumBinaryLeft == 0)
            {
                // Go back to reading ASCII
                mNumBinaryParsed  = -1;
            }

            if (!mOverflowDetected)
            {
                mParserRx.push_back(*chars);
            }
        }
        else if ((*chars) == BINARY_START_CHAR)
        {
            mNumBinaryParsed = 0;
            mStoredBinarySize = 0;
            mNumBinaryLeft = 2; // read at least 2 characters for size

            if (!mOverflowDetected)
            {
                mParserRx.push_back(*chars);
            }

            mLastIsEol = false;
        }
        else if (mOverflowDetected)
        {
            if (strchr(INPUT_EOL_CHARS, *chars) != NULL)
            {
                printf("Error: Command input overflow %lu\n", (long unsigned int)mParserRx.size());
                // Remove only command that overflowed
                if (mEndMarkers.empty())
                {
                    mParserRx.clear();
                }
                else
                {
                    // We still captured one or more full commands, so at least leave that for processing
                    std::size_t pos = mEndMarkers.back();
                    std::vector<char>::iterator lastEnd = mParserRx.begin() + pos;
                    mParserRx.erase(lastEnd + 1, mParserRx.end());
                }
                mOverflowDetected = false;
                mLastIsEol = true;
            }
            else
            {
                mLastIsEol = false;
            }
        }
        else if (strchr(BACKSPACE_CHARS, *chars) != NULL)
        {
            // Can't backspace before EOL character or if list is empty
            if (!mLastIsEol && !mParserRx.empty())
            {
                // Backspace
                mParserRx.pop_back();
            }
        }
        else if (strchr(INPUT_EOL_CHARS, *chars) != NULL)
        {
            if (!mLastIsEol)
            {
                mEndMarkers.push_back(mParserRx.size());
                mParserRx.push_back('\0');
                mLastIsEol = true;
            }
        }
        else
        {
            mParserRx.push_back(*chars);
            mLastIsEol = false;
        }
    }
}

void SerialStreamParser::process()
{
    // Only do something if a command is ready
    if (!mEndMarkers.empty())
    {
        // Begin lock guard context
        LockGuard lockGuard(mParserMutex);

        // End of command is at the new line character
        std::size_t pos = mEndMarkers.front();
        std::vector<char>::iterator eol = mParserRx.begin() + pos;

        if (eol != mParserRx.end())
        {
            // Move past whitespace characters
            const char* ptr = &mParserRx[0];
            uint32_t len = eol - mParserRx.begin();
            while (len > 0 && strchr(WHITESPACE_CHARS, *ptr) != NULL)
            {
                --len;
                ++ptr;
            }

            if (len > 0)
            {
                if (*ptr == mHelpChar)
                {
                    printf("HELP\n"
                           "Command structure: [whitespace]<command-char>[command]<\\n>\n"
                           "\n"
                           "COMMANDS:\n");
                    printf("%c: Prints this help\n", mHelpChar);
                    // Print help for all commands
                    for (std::vector<std::shared_ptr<CommandParser>>::iterator iter = mParsers.begin();
                        iter != mParsers.end();
                        ++iter)
                    {
                        (*iter)->printHelp();
                    }
                }
                else
                {
                    // Find command parser that can process this command
                    bool processed = false;
                    for (std::vector<std::shared_ptr<CommandParser>>::iterator iter = mParsers.begin();
                        iter != mParsers.end() && !processed;
                        ++iter)
                    {
                        if (strchr((*iter)->getCommandChars(), *ptr) != NULL)
                        {
                            (*iter)->submit(ptr, len);
                            processed = true;
                        }
                    }

                    if (!processed)
                    {
                        printf("Error: Invalid command\n");
                    }
                }
            }
            // Else: empty string - do nothing

            mEndMarkers.pop_front();
            mParserRx.erase(mParserRx.begin(), eol + 1);
            mParserRx.shrink_to_fit();
            for (std::size_t& s : mEndMarkers)
            {
                s -= (pos + 1);
            }
        }
    } // End lock guard context
}

std::size_t SerialStreamParser::numBufferedChars()
{
    return mParserRx.size();
}

std::size_t SerialStreamParser::numBufferedCmds()
{
    return mEndMarkers.size();
}
