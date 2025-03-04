#pragma once

#include "hal/Usb/CommandParser.hpp"
#include "hal/System/SystemIdentification.hpp"
#include "hal/System/MutexInterface.hpp"

#include "PrioritizedTxScheduler.hpp"

#include "PlayerData.hpp"
#include "DreamcastMainNode.hpp"

#include <memory>

// Command structure: [whitespace]<command-char>[command]<\n>

// Simple definition of a transmitter which just echos status and received data
class FlycastEchoTransmitter : public Transmitter
{
public:
    FlycastEchoTransmitter(MutexInterface& m);

    virtual ~FlycastEchoTransmitter() = default;

    virtual void txStarted(std::shared_ptr<const Transmission> tx) final;

    virtual void txFailed(bool writeFailed,
                          bool readFailed,
                          std::shared_ptr<const Transmission> tx) final;

    virtual void txComplete(std::shared_ptr<const MaplePacket> packet,
                            std::shared_ptr<const Transmission> tx) final;

private:
    MutexInterface& mMutex;
};

class FlycastBinaryEchoTransmitter : public Transmitter
{
public:
    FlycastBinaryEchoTransmitter(MutexInterface& m);

    virtual ~FlycastBinaryEchoTransmitter() = default;

    virtual void txStarted(std::shared_ptr<const Transmission> tx) final;

    virtual void txFailed(bool writeFailed,
                          bool readFailed,
                          std::shared_ptr<const Transmission> tx) final;

    virtual void txComplete(std::shared_ptr<const MaplePacket> packet,
                            std::shared_ptr<const Transmission> tx) final;

private:
    MutexInterface& mMutex;
};

//! Command parser for commands from flycast emulator
class FlycastCommandParser : public CommandParser
{
public:
    FlycastCommandParser(
        MutexInterface& m,
        SystemIdentification& identification,
        std::shared_ptr<PrioritizedTxScheduler>* schedulers,
        const uint8_t* senderAddresses,
        uint32_t numSenders,
        const std::vector<std::shared_ptr<PlayerData>>& playerData,
        const std::vector<std::shared_ptr<DreamcastMainNode>>& nodes);

    //! @returns the string of command characters this parser handles
    virtual const char* getCommandChars() final;

    //! Called when newline reached; submit command and reset
    virtual void submit(const char* chars, uint32_t len) final;

    //! Prints help message for this command
    virtual void printHelp() final;

private:
    static const char* INTERFACE_VERSION;
    MutexInterface& mMutex;
    SystemIdentification& mIdentification;
    std::shared_ptr<PrioritizedTxScheduler>* const mSchedulers;
    const uint8_t* const mSenderAddresses;
    const uint32_t mNumSenders;
    std::vector<std::shared_ptr<PlayerData>> mPlayerData;
    std::vector<std::shared_ptr<DreamcastMainNode>> nodes;
    std::unique_ptr<FlycastEchoTransmitter> mFlycastEchoTransmitter;
    std::unique_ptr<FlycastBinaryEchoTransmitter> mFlycastBinaryEchoTransmitter;
};
