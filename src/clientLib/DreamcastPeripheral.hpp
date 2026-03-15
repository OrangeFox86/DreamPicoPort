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

#include <memory>
#include <map>

#include "hal/MapleBus/MaplePacket.hpp"
#include "hal/MapleBus/MapleBusInterface.hpp"
#include "DreamcastPeripheralFunction.hpp"

namespace client
{

class DreamcastPeripheral
{
public:
    DreamcastPeripheral() = delete;

    //! Constructor
    //! @param[in] addr  Address (mask) of this peripheral
    DreamcastPeripheral(uint8_t addr,
                        uint8_t regionCode,
                        uint8_t connectionDirectionCode,
                        const char* descriptionStr,
                        const char* producerStr,
                        const char* versionStr,
                        float standbyCurrentmA,
                        float maxCurrentmA);

    DreamcastPeripheral(uint8_t addr,
                        uint8_t regionCode,
                        uint8_t connectionDirectionCode,
                        const char* descriptionStr,
                        const char* versionStr,
                        float standbyCurrentmA,
                        float maxCurrentmA);

    //! Virtual destructor
    virtual inline ~DreamcastPeripheral() {}

    //! Handles a packet from the Maple Bus
    //! @param[in] in  The packet read from the Maple Bus
    //! @param[out] out  The packet to write to the Maple Bus when true is returned
    //! @returns true iff the packet was handled
    virtual bool handlePacket(const MaplePacket& in, MaplePacket& out);

    //! Called when player index changed, timeout occurred, or reset command received
    virtual void reset();

    //! Called when shutdown command received
    virtual void shutdown();

    //! Add a function
    //! @param[in] fn  The function to add
    void addFunction(std::shared_ptr<DreamcastPeripheralFunction> fn);

    //! Remove a previously added function
    //! @param[in] functionCode  The function code of the function to remove
    bool removeFunction(uint32_t functionCode);

    //! Sets my address augmenter
    inline void setAddrAugmenter(uint8_t augmenter) { mAddrAugmenter = augmenter; }

    //! @returns true iff this peripheral is connected to host
    inline bool isConnected() { return mConnected; }

    //! @returns address of this peripheral
    inline uint8_t getAddress() { return (mAddr | mAddrAugmenter); }

private:
    //! Sets a string in device info array
    //! @param[in] wordIdx  The word index in device info array where string starts
    //! @param[in] offset  The byte offset in the word [0,3]
    //! @param[in] len  The number of bytes reserved for the string
    //! @param[in] str  The null-terminated string to set
    void setDevInfoStr(uint8_t wordIdx, uint8_t offset, uint8_t len, const char* str);

    //! Sets the function definitions within device info array based on all added functions
    void setDevInfoFunctionDefinitions();

public:
    static const uint8_t PLAYER_ID_ADDR_MASK = 0xC0;
    static const uint8_t PLAYER_ID_BIT_SHIFT = 6;
    //! Maximum allowed functions in a Dreamcast peripheral (due to device info message limitations)
    static const uint8_t MAX_NUMBER_OF_FUNCTIONS = 3;
    //! Address (mask) of this peripheral
    const uint8_t mAddr;

protected:
    //! true iff "connected"
    bool mConnected;
    //! Address augmentation mask for bus number and main peripheral sub mask
    uint8_t mAddrAugmenter;

private:
    //! Stores between 1 and 3 peripheral functions
    std::map<uint32_t, std::shared_ptr<DreamcastPeripheralFunction>> mDevices;
    //! Device info array
    uint32_t mDevInfo[48];
};

}
