#include "FlycastCommandParser.hpp"
#include "hal/MapleBus/MaplePacket.hpp"
#include "hal/System/LockGuard.hpp"

#include <stdio.h>
#include <cctype>
#include <cstring>
#include <string>
#include <cstdlib>

// Flycast command parser
// Format: X[modifier-char]<cmd-data>\n
// This parser must always return a single line of data

const char* FlycastCommandParser::INTERFACE_VERSION = "1.00";

static void send_response(const std::string& response)
{
    usb_cdc_write(response.c_str(), response.size());
}

static void send_response(const char* response)
{
    usb_cdc_write(response, strlen(response));
}

static void send_response(const char* response, int length)
{
    usb_cdc_write(response, length);
}

static void send_response(char response)
{
    usb_cdc_write(&response, 1);
}


FlycastEchoTransmitter::FlycastEchoTransmitter(MutexInterface& m) : mMutex(m) {}

void FlycastEchoTransmitter::txStarted(std::shared_ptr<const Transmission> tx)
{}

void FlycastEchoTransmitter::txFailed(
    bool writeFailed,
    bool readFailed,
    std::shared_ptr<const Transmission> tx)
{
    LockGuard lock(mMutex);

    if (writeFailed)
    {
        send_response("*failed write\n");
    }
    else
    {
        send_response("*failed read\n");
    }
}

void FlycastEchoTransmitter::txComplete(
    std::shared_ptr<const MaplePacket> packet,
    std::shared_ptr<const Transmission> tx)
{
    LockGuard lock(mMutex);

    char buf[64];
    snprintf(buf, 64,
        "%02hhX %02hhX %02hhX %02hhX",
        packet->frame.command,
        packet->frame.recipientAddr,
        packet->frame.senderAddr,
        packet->frame.length);

    send_response(buf);

    for (uint32_t p : packet->payload)
    {
        snprintf(buf, 64, " %08lX", p);
        send_response(buf);
    }

    send_response("\n");
}

FlycastBinaryEchoTransmitter::FlycastBinaryEchoTransmitter(MutexInterface& m) : mMutex(m) {}

void FlycastBinaryEchoTransmitter::txStarted(std::shared_ptr<const Transmission> tx)
{}

void FlycastBinaryEchoTransmitter::txFailed(
    bool writeFailed,
    bool readFailed,
    std::shared_ptr<const Transmission> tx)
{
    LockGuard lock(mMutex);

    if (writeFailed)
    {
        send_response("*failed write\n");
    }
    else
    {
        send_response("*failed read\n");
    }
}

void FlycastBinaryEchoTransmitter::txComplete(
    std::shared_ptr<const MaplePacket> packet,
    std::shared_ptr<const Transmission> tx)
{
    LockGuard lock(mMutex);

    send_response(CommandParser::BINARY_START_CHAR);
    uint16_t len = 4 + (packet->payload.size() * 4);
    send_response(static_cast<uint8_t>(len >> 8));
    send_response(static_cast<uint8_t>(len & 0xFF));
    uint8_t frame[4] = {
        packet->frame.command,
        packet->frame.recipientAddr,
        packet->frame.senderAddr,
        packet->frame.length
    };
    send_response(reinterpret_cast<char*>(frame), 4);

    for (uint32_t p : packet->payload)
    {
        uint8_t word[4] = {
            static_cast<uint8_t>(p >> 24),
            static_cast<uint8_t>((p >> 16) & 0xFF),
            static_cast<uint8_t>((p >> 8) & 0xFF),
            static_cast<uint8_t>(p & 0xFF)
        };
        send_response(reinterpret_cast<char*>(word), 4);
    }

    send_response('\n');
}

FlycastCommandParser::FlycastCommandParser(
    MutexInterface& m,
    SystemIdentification& identification,
    std::shared_ptr<PrioritizedTxScheduler>* schedulers,
    const uint8_t* senderAddresses,
    uint32_t numSenders,
    const std::vector<std::shared_ptr<PlayerData>>& playerData,
    const std::vector<std::shared_ptr<DreamcastMainNode>>& nodes
) :
    mMutex(m),
    mIdentification(identification),
    mSchedulers(schedulers),
    mSenderAddresses(senderAddresses),
    mNumSenders(numSenders),
    mPlayerData(playerData),
    nodes(nodes),
    mSummaryCallback(std::bind(&FlycastCommandParser::summaryCallback, this, std::placeholders::_1))
{
    mFlycastEchoTransmitter = std::make_unique<FlycastEchoTransmitter>(mMutex);
    mFlycastBinaryEchoTransmitter = std::make_unique<FlycastBinaryEchoTransmitter>(mMutex);
}

const char* FlycastCommandParser::getCommandChars()
{
    // X is reserved for command from flycast emulator
    return "X";
}

uint32_t parseWord(const char*& iter, const char* eol, uint32_t& i)
{
    uint32_t word = 0;
    i = 0;
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

    if (i == 8)
    {
        return word;
    }
    return 0;
}

void FlycastCommandParser::submit(const char* chars, uint32_t len)
{
    if (len == 0)
    {
        // This shouldn't happen, but handle it regardless
        return;
    }

    bool binaryParsed = false;
    bool valid = false;
    const char* eol = chars + len;
    MaplePacket::Frame frameWord = MaplePacket::Frame::defaultFrame();
    std::vector<uint32_t> payloadWords;
    const char* iter = chars + 1; // Skip past 'X' (implied)

    // left strip
    while (iter < eol && std::isspace(*iter))
    {
        ++iter;
    }

    if (*iter != BINARY_START_CHAR)
    {
        // right strip
        while (iter < eol && std::isspace(*(eol - 1)))
        {
            --eol;
        }
    }

    // Check for special commanding
    if (iter < eol)
    {
        switch(*iter)
        {
            // Either X- to reset all or one of {X-0, X-1, X-2, X-3} to reset a specific player
            case '-':
            {
                // Remove minus
                ++iter;
                int idx = -1;
                if (iter < eol)
                {
                    std::string number;
                    number.assign(iter, eol - iter);
                    if (0 == sscanf(iter, "%i", &idx))
                    {
                        idx = -1;
                    }
                }

                LockGuard lock(mMutex);

                // Reset screen data
                if (idx < 0)
                {
                    // all
                    int count = 0;
                    for (std::shared_ptr<PlayerData>& playerData : mPlayerData)
                    {
                        ++count;
                        playerData->screenData.resetToDefault();
                    }
                    std::string s = std::to_string(count);
                    send_response(std::to_string(count));
                }
                else if (static_cast<std::size_t>(idx) < mPlayerData.size())
                {
                    mPlayerData[idx]->screenData.resetToDefault();
                    send_response("1\n");
                }
                else
                {
                    send_response("0\n");

                }
            }
            return;

            // XP [0-4] [0-4] to change displayed port character
            case 'P':
            {
                LockGuard lock(mMutex);

                // Remove P
                ++iter;
                int idxin = -1;
                int idxout = -1;
                if (2 == sscanf(iter, "%i %i", &idxin, &idxout) &&
                    idxin >= 0 &&
                    static_cast<std::size_t>(idxin) < mPlayerData.size() &&
                    idxout >= 0 &&
                    static_cast<std::size_t>(idxout) < ScreenData::NUM_DEFAULT_SCREENS)
                {
                    mPlayerData[idxin]->screenData.setDataToADefault(idxout);
                    send_response("1\n");
                }
                else
                {
                    send_response("0\n");
                }
            }
            return;

            // XS to return serial
            case 'S':
            {
                LockGuard lock(mMutex);

                char buffer[mIdentification.getSerialSize() + 1] = {0};
                mIdentification.getSerial(buffer, sizeof(buffer) - 1);
                buffer[sizeof(buffer) - 1] = '\0';
                send_response(buffer);
                send_response("\n");
            }
            return;

            // X?0, X?1, X?2, or X?3 will print summary for the given node index
            case '?':
            {
                // Remove question mark
                ++iter;
                int idx = -1;
                if (iter < eol)
                {
                    std::string number;
                    number.assign(iter, eol - iter);
                    if (0 == sscanf(iter, "%i", &idx))
                    {
                        idx = -1;
                    }
                }

                if (idx >= 0 && static_cast<std::size_t>(idx) < nodes.size())
                {
                    // NOTE: Mutex will be taken in the callback
                    nodes[idx]->requestSummary(mSummaryCallback);
                }
                else
                {
                    LockGuard lock(mMutex);
                    send_response("NULL\n");
                }
            }
            return;

            // XV to return interface version
            case 'V':
            {
                LockGuard lock(mMutex);
                send_response(INTERFACE_VERSION);
                send_response('\n');
            }
            return;

            // XH0 for echo off, XH1 for echo on
            case 'H':
            {
                // Remove E
                ++iter;
                int on = -1;
                if (iter < eol)
                {
                    std::string number;
                    number.assign(iter, eol - iter);
                    if (0 == sscanf(iter, "%i", &on))
                    {
                        on = -1;
                    }
                }

                LockGuard lock(mMutex);

                if (on == 1)
                {
                    usb_cdc_set_echo(true);
                    send_response("ECHO ON\n");
                }
                else if (on == 0)
                {
                    usb_cdc_set_echo(false);
                    send_response("ECHO OFF\n");
                }
                else
                {
                    send_response("*failed invalid data\n");
                }
            }
            return;

            // Handle command as binary instead of ASCII
            case BINARY_START_CHAR:
            {
                // Remove binary start character
                ++iter;

                // Get size of data
                int32_t size = ((*iter) << 8) | (*(iter + 1));
                iter += 2;

                if (size >= 4)
                {
                    frameWord.command = *iter++;
                    frameWord.recipientAddr = *iter++;
                    frameWord.senderAddr = *iter++;
                    frameWord.length = *iter++;
                    size -= 4;

                    payloadWords.reserve(size / 4);

                    while (((iter + 4) <= eol) && (size >= 4))
                    {
                        uint32_t word =
                            (static_cast<uint32_t>(*iter) << 24) |
                            (static_cast<uint32_t>(*(iter + 1)) << 16) |
                            (static_cast<uint32_t>(*(iter + 2)) << 8) |
                            static_cast<uint32_t>(*(iter + 3));

                        payloadWords.push_back(word);

                        iter += 4;
                        size -= 4;
                    }

                    binaryParsed = true;
                    valid = (size == 0);
                }
                else
                {
                    valid = false;
                }

                iter = eol;
            }
            break; // break out to parsing below

            // Reserved
            case ' ': // Fall through
            case '0': // Fall through
            case '1': // Fall through
            case '2': // Fall through
            case '3': // Fall through
            case '4': // Fall through
            case '5': // Fall through
            case '6': // Fall through
            case '7': // Fall through
            case '8': // Fall through
            case '9': // Fall through
            case 'a': // Fall through
            case 'b': // Fall through
            case 'c': // Fall through
            case 'd': // Fall through
            case 'e': // Fall through
            case 'f': // Fall through
            case 'A': // Fall through
            case 'B': // Fall through
            case 'C': // Fall through
            case 'D': // Fall through
            case 'E': // Fall through
            case 'F': // Fall through

            // No special case
            default: break;
        }
    }

    if (!binaryParsed)
    {
        uint32_t i = 0;
        uint32_t firstWord = parseWord(iter, eol, i);
        if (i == 8)
        {
            frameWord = MaplePacket::Frame::fromWord(firstWord);
            valid = true;

            while (iter < eol)
            {
                uint32_t word = parseWord(iter, eol, i);

                // Invalid if a partial word was given
                valid = ((i == 8) || (i == 0));

                if (i == 8)
                {
                    payloadWords.push_back(word);
                }
            }
        }
        else
        {
            valid = false;
        }
    }

    if (valid)
    {
        MaplePacket packet(frameWord, std::move(payloadWords));
        if (packet.isValid())
        {
            uint8_t sender = packet.frame.senderAddr;
            int32_t idx = -1;
            const uint8_t* senderAddress = mSenderAddresses;

            if (mNumSenders == 1)
            {
                // Single player special case - always send to the one available, regardless of address
                idx = 0;
                packet.frame.senderAddr = *senderAddress;
                packet.frame.recipientAddr = (packet.frame.recipientAddr & 0x3F) | *senderAddress;
            }
            else
            {
                for (uint32_t i = 0; i < mNumSenders && idx < 0; ++i, ++senderAddress)
                {
                    if (sender == *senderAddress)
                    {
                        idx = i;
                    }
                }
            }

            if (idx >= 0)
            {
                if (packet.frame.command == 0x0C &&
                    packet.frame.length == 0x32 &&
                    packet.payload[0] == DEVICE_FN_LCD &&
                    packet.payload[1] == 0)
                {
                    // Save screen data
                    mPlayerData[idx]->screenData.setData(&packet.payload[2], 0, 0x30, false);
                }

                Transmitter* t;
                if (binaryParsed)
                {
                    t = mFlycastBinaryEchoTransmitter.get();
                }
                else
                {
                    t = mFlycastEchoTransmitter.get();
                }

                mSchedulers[idx]->add(
                    PrioritizedTxScheduler::EXTERNAL_TRANSMISSION_PRIORITY,
                    PrioritizedTxScheduler::TX_TIME_ASAP,
                    t,
                    packet,
                    true);
            }
            else
            {
                LockGuard lock(mMutex);
                send_response("*failed invalid sender\n");
            }
        }
        else
        {
            LockGuard lock(mMutex);
            send_response("*failed packet invalid\n");
        }
    }
    else
    {
        LockGuard lock(mMutex);
        send_response("*failed missing data\n");
    }
}

void FlycastCommandParser::printHelp()
{
    send_response("X: commands from a flycast emulator\n");
}

void FlycastCommandParser::summaryCallback(const std::list<std::list<std::array<uint32_t, 2>>>& summary)
{
    std::string summaryString;
    summaryString.reserve(512);

    bool firstI = true;
    for (const auto& i : summary)
    {
        if (!firstI)
        {
            summaryString += ';';
        }

        bool firstJ = true;
        for (const auto& j : i)
        {
            if (!firstJ)
            {
                summaryString += ',';
            }

            summaryString += '{';

            char buffer[10];
            snprintf(buffer, sizeof(buffer), "%08lX,", j[0]);
            summaryString += buffer;
            snprintf(buffer, sizeof(buffer), "%08lX", j[1]);
            summaryString += buffer;

            summaryString += '}';

            firstJ = false;
        }

        firstI = false;
    }

    summaryString += '\n';

    LockGuard lock(mMutex);
    send_response(summaryString);
}
