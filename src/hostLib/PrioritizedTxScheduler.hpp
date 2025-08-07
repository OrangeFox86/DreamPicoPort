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

#pragma once

#include "hal/MapleBus/MaplePacket.hpp"
#include "hal/System/MutexInterface.hpp"
#include "dreamcast_constants.h"
#include "Transmission.hpp"
#include <list>
#include <vector>
#include <memory>

class PrioritizedTxScheduler
{
public:
    //! Enumerates available priorities to be used with add()
    enum Priority : uint8_t
    {
        //! Priority for external entity taking control of the bus (max)
        EXTERNAL_TRANSMISSION_PRIORITY = 0,
        //! Priority for main peripheral
        MAIN_TRANSMISSION_PRIORITY,
        //! Priority for sub peripheral (min)
        SUB_TRANSMISSION_PRIORITY,
        //! Any selected priority must be less than PRIORITY_COUNT
        PRIORITY_COUNT
    };

    //! Points to a schedule item within the current schedule
    class ScheduleItem
    {
        //! Only the PrioritizedTxScheduler may modify private elements here
        friend PrioritizedTxScheduler;

        public:
            //! Constructor
            ScheduleItem() : mIsValid(false), mTime(0) {}

            //! @returns the transmission for this schedule item
            std::shared_ptr<Transmission> getTx() {return mIsValid ? *mItemIter : nullptr;}

        private:
            //! Set to true iff iterators are valid
            bool mIsValid;
            //! The schedule group
            std::vector<std::list<std::shared_ptr<Transmission>>>::iterator mScheduleIter;
            //! The item within the schedule group
            std::list<std::shared_ptr<Transmission>>::iterator mItemIter;
            //! The time at which this item was peeked
            uint64_t mTime;
    };

    //! Contains properties for a transmission
    struct TransmissionProperties
    {
        //! Priority of this transmission (0 is highest priority)
        uint8_t priority;
        //! Time at which this should transmit in microseconds
        uint64_t txTime;
        //! Packet data to send (internal data is moved out of this object on add())
        MaplePacket packet;
        //! true iff a response is expected after transmission
        bool expectResponse;
        //! Number of payload words to expect in response (used for priority timing calculations)
        uint32_t expectedResponseNumPayloadWords=0;
        //! How often to repeat this transmission in microseconds
        uint32_t autoRepeatUs=0;
        //! If not 0, auto repeat will cancel after this time
        uint64_t autoRepeatEndTimeUs=0;
        //! The desired byte order of the response
        MaplePacket::ByteOrder rxByteOrder=MaplePacket::ByteOrder::HOST;
    };

public:
    //! Default constructor
    //! @param[in] senderAddress  The sender address set in every packet added
    //! @param[in] max  The maximum accepted priority
    PrioritizedTxScheduler(MutexInterface& m, uint8_t senderAddress, uint32_t max = (PRIORITY_COUNT-1));

    //! Virtual destructor
    virtual ~PrioritizedTxScheduler();

    //! Add a transmission to the schedule
    //! @param[in] properties  The selected properties for this transmission
    //! @param[in] transmitter  Pointer to transmitter that is adding this
    //! @returns transmission ID
    uint32_t add(
        TransmissionProperties properties,
        Transmitter* transmitter
    );

    //! Add a transmission to the schedule
    //! @param[in] properties  The selected properties for this transmission
    //! @param[in] transmitter  Pointer to transmitter that is adding this (kept alive until transmission completes)
    //! @returns transmission ID
    uint32_t add(
        TransmissionProperties properties,
        const std::shared_ptr<Transmitter>& transmitter
    );

    //! Peeks the next scheduled packet, given the current time
    //! @param[in] time  The current time
    //! @returns nullptr if no scheduled packet is available for the given time
    //! @returns the next scheduled item for the given current time
    ScheduleItem peekNext(uint64_t time);

    //! Pops a schedule item that was retrieved using peekNext
    //! @param[in,out] scheduleItem  The schedule item to pop and invalidate
    std::shared_ptr<Transmission> popItem(ScheduleItem& scheduleItem);

    //! Cancels scheduled transmission by transmission ID
    //! @param[in] transmissionId  The transmission ID of the transmissions to cancel
    //! @returns number of transmissions successfully canceled
    uint32_t cancelById(uint32_t transmissionId);

    //! Cancels scheduled transmission by recipient address
    //! @param[in] recipientAddr  The recipient address of the transmissions to cancel
    //! @returns number of transmissions successfully canceled
    uint32_t cancelByRecipient(uint8_t recipientAddr);

    //! Count how many scheduled transmissions have a given recipient address
    //! @param[in] recipientAddr  The recipient address
    //! @returns the number of transmissions have the given recipient address
    uint32_t countRecipients(uint8_t recipientAddr);

    //! Cancels all items in the schedule
    //! @returns number of transmissions successfully canceled
    uint32_t cancelAll();

    //! Computes the next time on a cadence
    //! @param[in] currentTime  The current time
    //! @param[in] period  The period at which this item is scheduled (must be > 0)
    //! @param[in] offset  The offset that this item began or previously executed at
    //! @returns the next time in the future which is confined to period and offset
    static uint64_t computeNextTimeCadence(uint64_t currentTime,
                                           uint64_t period,
                                           uint64_t offset = 0);

protected:
    //! Add a transmission to the schedule
    //! @param[in] tx  The transmission to add
    //! @returns transmission ID
    uint32_t add(std::shared_ptr<Transmission> tx);

public:
    //! Use this for txTime if the packet needs to be sent ASAP
    static const uint64_t TX_TIME_ASAP = 0;
    //! Transmission ID to use in order to flag no ID
    static const uint32_t INVALID_TX_ID = 0;

protected:
    //! Mutex used to serialize push/pop of external items
    MutexInterface& mScheduleMutex;
    //! The address of this sender
    const uint8_t mSenderAddress;
    //! The next transmission ID to set
    uint32_t mNextId;
    //! The current schedule ordered by priority and time
    std::vector<std::list<std::shared_ptr<Transmission>>> mSchedule;
};