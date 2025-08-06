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

#include "MaplePassthroughCommandParser.hpp"
#include "hal/MapleBus/MaplePacket.hpp"

#include <stdio.h>

// Simple definition of a transmitter which just echos status and received data
class EchoTransmitter : public Transmitter
{
public:
    virtual void txStarted(std::shared_ptr<const Transmission> tx) final
    {}

    virtual void txFailed(bool writeFailed,
                          bool readFailed,
                          std::shared_ptr<Transmission> tx) final
    {
        if (writeFailed)
        {
            printf("%lu: failed write\n", (long unsigned int)tx->transmissionId);
        }
        else
        {
            printf("%lu: failed read\n", (long unsigned int)tx->transmissionId);
        }
    }

    virtual void txComplete(std::shared_ptr<MaplePacket> packet,
                            std::shared_ptr<Transmission> tx) final
    {
        printf("%lu: complete {", (long unsigned int)tx->transmissionId);
        printf("%08lX", (long unsigned int)packet->frame.toWord());
        for (std::vector<uint32_t>::const_iterator iter = packet->payload.begin();
             iter != packet->payload.end();
             ++iter)
        {
            printf(" %08lX", (long unsigned int)*iter);
        }
        printf("}\n");
    }
} echoTransmitter;

MaplePassthroughCommandParser::MaplePassthroughCommandParser(std::shared_ptr<PrioritizedTxScheduler>* schedulers,
                                                             const uint8_t* senderAddresses,
                                                             uint32_t numSenders) :
    mSchedulers(schedulers),
    mSenderAddresses(senderAddresses),
    mNumSenders(numSenders)
{}

const char* MaplePassthroughCommandParser::getCommandChars()
{
    // Anything beginning with a hex character should be considered a passthrough command
    return "0123456789ABCDEFabcdef";
}

void MaplePassthroughCommandParser::submit(const char* chars, uint32_t len)
{
    bool valid = false;
    const char* const eol = chars + len;
    std::vector<uint32_t> words;
    const char* iter = chars;
    while(iter < eol)
    {
        uint32_t word = 0;
        uint32_t i = 0;
        while (i < 8 && iter < eol)
        {
            char v = *iter++;
            uint_fast8_t value = 0;

            if (v >= '0' && v <= '9')
            {
                value = v - '0';
            }
            else if (v >= 'a' && v <= 'f')
            {
                value = v - 'a' + 0xa;
            }
            else if (v >= 'A' && v <= 'F')
            {
                value = v - 'A' + 0xA;
            }
            else
            {
                // Ignore this character
                continue;
            }

            // Apply value into current word
            word |= (value << ((8 - i) * 4 - 4));
            ++i;
        }

        // Invalid if a partial word was given
        valid = ((i == 8) || (i == 0));

        if (i == 8)
        {
            words.push_back(word);
        }
    }

    if (valid)
    {
        MaplePacket packet(&words[0], words.size());
        if (packet.isValid())
        {
            uint8_t sender = packet.frame.senderAddr;
            int32_t idx = -1;
            const uint8_t* senderAddress = mSenderAddresses;

            for (uint32_t i = 0; i < mNumSenders && idx < 0; ++i, ++senderAddress)
            {
                if (sender == *senderAddress)
                {
                    idx = i;
                }
            }

            if (idx >= 0)
            {
                uint32_t id = mSchedulers[idx]->add(
                    PrioritizedTxScheduler::EXTERNAL_TRANSMISSION_PRIORITY,
                    PrioritizedTxScheduler::TX_TIME_ASAP,
                    &echoTransmitter,
                    packet,
                    true);
                std::vector<uint32_t>::iterator iter = words.begin();
                printf("%lu: added {%08lX", (long unsigned int)id, (long unsigned int)*iter++);
                for(; iter < words.end(); ++iter)
                {
                    printf(" %08lX", (long unsigned int)*iter);
                }
                printf("} -> [%li]\n", (long int)idx);
            }
            else
            {
                printf("0: failed invalid sender\n");
            }
        }
        else
        {
            printf("0: failed packet invalid\n");
        }
    }
    else
    {
        printf("0: failed missing data\n");
    }
}

void MaplePassthroughCommandParser::printHelp()
{
    printf("0-1 a-f A-F: the beginning of a hex value to send to maple bus without CRC\n");
}
