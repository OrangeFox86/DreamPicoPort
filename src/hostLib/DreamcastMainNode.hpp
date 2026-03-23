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

#include "DreamcastNode.hpp"
#include "DreamcastSubNode.hpp"
#include "hal/MapleBus/MapleBusInterface.hpp"
#include "DreamcastPeripheral.hpp"
#include "TransmissionTimeliner.hpp"

#include <memory>
#include <vector>
#include <functional>
#include <tuple>

//! Handles communication for the main Dreamcast node for a single bus. In other words, this
//! facilitates communication to test for and identify a main peripheral such as a controller and
//! routes received communication to that peripheral or a sub node under this.
class DreamcastMainNode : public DreamcastNode
{
    public:
        //! Constructor
        //! @param[in] bus  The bus on which this node communicates
        //! @param[in] playerData  The player data passed to any connected peripheral
        //! @param[in] prioritizedTxScheduler The scheduler handling Maple Bus commands
        //! @param[in] detectionOnly When true, poll the node until its presence is detected
        DreamcastMainNode(
            const std::shared_ptr<MapleBusInterface>& bus,
            const std::shared_ptr<PlayerData>& playerData,
            const std::shared_ptr<PrioritizedTxScheduler>& prioritizedTxScheduler,
            bool detectionOnly = false
        );

        //! Virtual destructor
        virtual ~DreamcastMainNode();

        //! Inherited from DreamcastNode
        virtual void task(uint64_t currentTimeUs) final;

        //! Inherited from DreamcastNode
        virtual inline void txStarted(std::shared_ptr<const Transmission> tx) final
        {}

        //! Inherited from DreamcastNode
        virtual inline void txFailed(bool writeFailed,
                                     bool readFailed,
                                     std::shared_ptr<const Transmission> tx) final
        {}

        //! Inherited from DreamcastNode
        virtual void txComplete(std::shared_ptr<const MaplePacket> packet,
                                std::shared_ptr<const Transmission> tx) final;

        //! Called when the main peripheral needs to be disconnected
        //! @param[in] currentTimeUs  The current time as number of microseconds
        void disconnectMainPeripheral(uint64_t currentTimeUs);

        //! Prints summary of all devices
        void printSummary();

        //! @return a summary of what peripherals are connected. The outer list explain each main node. The inner list
        //!         explain each peripheral. The inner array index [0] is function code and [1] is function definitions
        //!         word.
        std::list<std::list<std::array<uint32_t, 2>>> getSummary() const;

        //! Contains MapleBus status data
        struct MapleStatus
        {
            //! Last received phase of the state machine
            MapleBusInterface::Phase phase;
            //! The current statistics for the MapleBus
            MapleBusInterface::MapleStats mapleStats;
        };

        //! @return current status and statistics information
        MapleStatus getMapleStatus() const;

        //! @return true iff device detected on this node
        inline bool isDeviceDetected()
        {
            return mDeviceDetected;
        }

    private:
        //! Execute and process read task from the timeliner
        //! @param[in] currentTimeUs  The current time in microseconds
        void readTask(uint64_t currentTimeUs);

        //! Run task of all of my dependents
        //! @param[in] currentTimeUs  The current time in microseconds
        void runDependentTasks(uint64_t currentTimeUs);

        //! Execute and process write task from the timeliner
        //! @param[in] currentTimeUs  The current time in microseconds
        void writeTask(uint64_t currentTimeUs);

        //! Adds an auto reload info request to the transmission schedule
        void addInfoRequestToSchedule(uint64_t currentTimeUs = 0);

        //! Cancel the auto reload info request from the transmission schedule
        void cancelInfoRequest();

        //! Called when one or more peripherals have been added or removed
        void peripheralChangeEvent(uint64_t currentTimeUs);

    public:
        //! Number of microseconds in between each info request when no peripheral is detected
        static const uint32_t US_PER_CHECK = 16000;
        //! Overrides the above when this node is used for detection only
        static const uint32_t DETECTION_ONLY_US_PER_CHECK = 250000;
        //! Number of communication failures before main peripheral is disconnected
        static const uint32_t MAX_FAILURE_DISCONNECT_COUNT = 10;
        //! Number of milliseconds that a connect signal is asserted
        static const uint32_t CONNECT_EVENT_SIGNAL_TIME_MS = 25;

    protected:
        //! True when the node only operates for detection only
        const bool mDetectionOnly;
        //! True when a device is detected on this node
        bool mDeviceDetected;
        //! The MapleBusInterface associated with this node
        std::shared_ptr<MapleBusInterface> mMapleBus;
        //! The sub nodes under this node
        std::vector<std::shared_ptr<DreamcastSubNode>> mSubNodes;
        //! Executes transmissions from the schedule
        TransmissionTimeliner mTransmissionTimeliner;
        //! ID of the device info request auto reload transmission this object added to the schedule
        int64_t mScheduleId;
        //! Current count of number of communication failures
        uint32_t mCommFailCount;
        //! Print summary on next cycle when true
        bool mPrintSummary;
        //! The last recorded phase of the MapleBusInterface
        MapleBusInterface::Phase mLastPhase;
        //! Set to true when connected signal should be sent on next task
        bool mSendConnectedSignal;
        //! The time at which the change signal should be released
        uint64_t mChangeReleaseTime;
};