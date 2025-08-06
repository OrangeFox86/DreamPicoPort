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

#include "DreamcastNode.hpp"

//! Handles communication for a Dreamcast sub node for a single bus.
class DreamcastSubNode : public DreamcastNode
{
    public:
        //! Constructor
        //! @param[in] addr  The address of this sub node
        //! @param[in] scheduler  The transmission scheduler this peripheral is to add to
        //! @param[in] playerData  The player data passed to any connected peripheral
        DreamcastSubNode(uint8_t addr,
                         std::shared_ptr<EndpointTxSchedulerInterface> scheduler,
                         PlayerData playerData);

        //! Copy constructor
        DreamcastSubNode(const DreamcastSubNode& rhs);

        //! Inherited from DreamcastNode
        virtual inline void txStarted(std::shared_ptr<const Transmission> tx)
        {}

        //! Inherited from DreamcastNode
        virtual inline void txFailed(bool writeFailed,
                                     bool readFailed,
                                     std::shared_ptr<Transmission> tx)
        {}

        //! Inherited from DreamcastNode
        virtual void txComplete(std::shared_ptr<MaplePacket> packet,
                                std::shared_ptr<Transmission> tx);

        //! Inherited from DreamcastNode
        virtual void task(uint64_t currentTimeUs);

        //! Called from the main node when a main peripheral disconnects. A main peripheral
        //! disconnecting should cause all sub peripherals to disconnect.
        virtual void mainPeripheralDisconnected();

        //! Called from the main node to update the connection state of peripherals on this sub node
        //! @return true iff state has changed
        virtual bool setConnected(bool connected, uint64_t currentTimeUs = 0);

    protected:
        //! Number of microseconds in between each info request when no peripheral is detected
        static const uint32_t US_PER_CHECK = 16000;
        //! Detected peripheral connection state
        bool mConnected;
        //! ID of the device info request auto reload transmission this object added to the schedule
        int64_t mScheduleId;

};
