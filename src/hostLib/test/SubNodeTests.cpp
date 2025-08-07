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

#include "MockMapleBus.hpp"
#include "MockDreamcastControllerObserver.hpp"
#include "MockDreamcastPeripheral.hpp"
#include "MockMutex.hpp"
#include "MockClock.hpp"
#include "MockUsbFileSystem.hpp"

#include "DreamcastMainNode.hpp"
#include "DreamcastSubNode.hpp"
#include "DreamcastPeripheral.hpp"
#include "dreamcast_constants.h"
#include "EndpointTxSchedulerInterface.hpp"
#include "EndpointTxScheduler.hpp"

#include <memory>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgReferee;
using ::testing::DoAll;

class DreamcastSubNodeOverride : public DreamcastSubNode
{
    public:
        DreamcastSubNodeOverride(uint8_t addr,
                                 std::shared_ptr<EndpointTxSchedulerInterface> scheduler,
                                 PlayerData playerData) :
            DreamcastSubNode(addr, scheduler, playerData)
        {
        }

        //! Called from peripheralFactory below so we can test what function code it was called with
        MOCK_METHOD(void, mockMethodPeripheralFactory, (const std::vector<uint32_t>& deviceInfoPayload));

        //! This function overrides the real peripheral factory so that mock peripherals may be
        //! created.
        uint32_t peripheralFactory(const std::vector<uint32_t>& deviceInfoPayload) override
        {
            mPeripherals = mPeripheralsToAdd;
            mockMethodPeripheralFactory(deviceInfoPayload);
            return 0;
        }

        //! Allows the test to check what peripherals the node has
        std::vector<std::shared_ptr<DreamcastPeripheral>>& getPeripherals()
        {
            return mPeripherals;
        }

        //! Allows the test to check connection state
        bool isConnected()
        {
            return mConnected;
        }

        //! Allows the test to set connection state
        void mockSetConnected(bool value)
        {
            mConnected = value;
        }

        //! Allows the test to set what peripherals to add on next call to peripheralFactory()
        std::vector<std::shared_ptr<DreamcastPeripheral>> mPeripheralsToAdd;
};

class SubNodeTest : public ::testing::Test
{
    public:
        //! Sets up the DreamcastMainNode with mocked interfaces
        SubNodeTest() :
            mDreamcastControllerObserver(),
            mMutex(),
            mScreenData(mMutex),
            mPlayerData{1, mDreamcastControllerObserver, mScreenData, mClock, mUsbFileSystem},
            mPrioritizedTxScheduler(std::make_shared<PrioritizedTxScheduler>(mMutex2, 0x00)),
            mEndpointTxScheduler(std::make_shared<EndpointTxScheduler>(
                mPrioritizedTxScheduler, 0, DreamcastPeripheral::getRecipientAddress(1, 0x01))),
            mDreamcastSubNode(0x01, mEndpointTxScheduler, mPlayerData)
        {}

    protected:
        MockDreamcastControllerObserver mDreamcastControllerObserver;
        MockMutex mMutex;
        MockMutex mMutex2;
        MockClock mClock;
        MockUsbFileSystem mUsbFileSystem;
        ScreenData mScreenData;
        PlayerData mPlayerData;
        std::shared_ptr<PrioritizedTxScheduler> mPrioritizedTxScheduler;
        std::shared_ptr<EndpointTxScheduler> mEndpointTxScheduler;
        DreamcastSubNodeOverride mDreamcastSubNode;
        std::shared_ptr<MockDreamcastPeripheral> mockDreamcastPeripheral1;
        std::shared_ptr<MockDreamcastPeripheral> mockDreamcastPeripheral2;

        virtual void SetUp()
        {}

        virtual void TearDown()
        {}

        void addTwoMockedPeripherals()
        {
            std::vector<std::shared_ptr<DreamcastPeripheral>>& peripherals =
                mDreamcastSubNode.getPeripherals();
            mockDreamcastPeripheral1 =
                std::make_shared<MockDreamcastPeripheral>(0x01, 0, mEndpointTxScheduler, mPlayerData.playerIndex);
            peripherals.push_back(mockDreamcastPeripheral1);
            mockDreamcastPeripheral2 =
                std::make_shared<MockDreamcastPeripheral>(0x01, 0, mEndpointTxScheduler, mPlayerData.playerIndex);
            peripherals.push_back(mockDreamcastPeripheral2);
        }
};

TEST_F(SubNodeTest, handleDataCommandNoPeripheralsAdded)
{
    // --- MOCKING ---
    EXPECT_CALL(mDreamcastSubNode, mockMethodPeripheralFactory(_)).Times(1);
    uint32_t payload[4] = {1234567, 0, 0, 0};
    std::shared_ptr<MaplePacket> packet = std::make_shared<MaplePacket>(
        MaplePacket::Frame{.command=5, .recipientAddr=0}, payload, 4);
    std::shared_ptr<MaplePacket> txPacket = std::make_shared<MaplePacket>(
        MaplePacket::Frame{.command=4, .recipientAddr=1}, 7654321);
    std::shared_ptr<const Transmission> tx =
        std::make_shared<Transmission>(0, 0, true, 123, 0, 0, 0, txPacket, MaplePacket::ByteOrder::HOST, nullptr);

    // --- TEST EXECUTION ---
    mDreamcastSubNode.txComplete(packet, tx);

    // --- EXPECTATIONS ---
    EXPECT_TRUE(mDreamcastSubNode.getPeripherals().empty());
}

TEST_F(SubNodeTest, handleDataCommandPeripheralAdded)
{
    // --- MOCKING ---
    // The mocked factory will add a mocked peripheral
    std::shared_ptr<MockDreamcastPeripheral> mockedDreamcastPeripheral =
        std::make_shared<MockDreamcastPeripheral>(0x01, 0, mEndpointTxScheduler, mPlayerData.playerIndex);
    mDreamcastSubNode.mPeripheralsToAdd.push_back(mockedDreamcastPeripheral);
    EXPECT_CALL(mDreamcastSubNode, mockMethodPeripheralFactory(_)).Times(1);
    uint32_t payload[4] = {1234567, 0, 0, 0};
    std::shared_ptr<MaplePacket> packet = std::make_shared<MaplePacket>(
        MaplePacket::Frame{.command=5, .recipientAddr=0}, payload, 4);
    std::shared_ptr<MaplePacket> txPacket = std::make_shared<MaplePacket>(
        MaplePacket::Frame{.command=4, .recipientAddr=1}, 7654321);
    std::shared_ptr<const Transmission> tx =
        std::make_shared<Transmission>(0, 0, true, 123, 0, 0, 0, txPacket, MaplePacket::ByteOrder::HOST, nullptr);

    // --- TEST EXECUTION ---
    mDreamcastSubNode.txComplete(packet, tx);

    // --- EXPECTATIONS ---
    EXPECT_EQ(mDreamcastSubNode.getPeripherals().size(), 1);
}

TEST_F(SubNodeTest, handleDataCommandInvalidPayloadSize)
{
    // --- MOCKING ---
    // The mocked factory will add a mocked peripheral
    std::shared_ptr<MockDreamcastPeripheral> mockedDreamcastPeripheral =
        std::make_shared<MockDreamcastPeripheral>(0x01, 0, mEndpointTxScheduler, mPlayerData.playerIndex);
    mDreamcastSubNode.mPeripheralsToAdd.push_back(mockedDreamcastPeripheral);
    std::shared_ptr<MaplePacket> packet = std::make_shared<MaplePacket>(
        MaplePacket::Frame{.command=5, .recipientAddr=0}, (uint32_t*)NULL, 0);
    std::shared_ptr<MaplePacket> txPacket = std::make_shared<MaplePacket>(
        MaplePacket::Frame{.command=4, .recipientAddr=1}, 7654321);
    std::shared_ptr<const Transmission> tx =
        std::make_shared<Transmission>(0, 0, true, 123, 0, 0, 0, txPacket, MaplePacket::ByteOrder::HOST, nullptr);
    EXPECT_CALL(mDreamcastSubNode, mockMethodPeripheralFactory(_)).Times(0);

    // --- TEST EXECUTION ---
    mDreamcastSubNode.txComplete(packet, tx);

    // --- EXPECTATIONS ---
    EXPECT_TRUE(mDreamcastSubNode.getPeripherals().empty());
}

TEST_F(SubNodeTest, taskPeripheralsConnectedSuccessfulTask)
{
    // --- MOCKING ---
    mDreamcastSubNode.mockSetConnected(true);
    // Add 2 mock peripherals to the sub node
    addTwoMockedPeripherals();
    // Both peripherals will successfully handle their task
    EXPECT_CALL(*mockDreamcastPeripheral1, task(554433)).Times(1);
    EXPECT_CALL(*mockDreamcastPeripheral2, task(554433)).Times(1);

    // --- TEST EXECUTION ---
    mDreamcastSubNode.task(554433);

    // --- EXPECTATIONS ---
    EXPECT_EQ(mDreamcastSubNode.getPeripherals().size(), 2);
}

TEST_F(SubNodeTest, mainPeripheralDisconnected)
{
    // --- MOCKING ---
    mDreamcastSubNode.mockSetConnected(true);
    // Add 2 mock peripherals to the sub node
    addTwoMockedPeripherals();

    // --- TEST EXECUTION ---
    mDreamcastSubNode.mainPeripheralDisconnected();

    // --- EXPECTATIONS ---
    EXPECT_TRUE(mDreamcastSubNode.getPeripherals().empty());
}

TEST_F(SubNodeTest, setConnectedDisconnectedToConnected)
{
    // --- MOCKING ---
    // Add 2 mock peripherals to the sub node
    addTwoMockedPeripherals();

    // --- TEST EXECUTION ---
    mDreamcastSubNode.setConnected(true);

    // --- EXPECTATIONS ---
    EXPECT_TRUE(mDreamcastSubNode.isConnected());
    EXPECT_EQ(mDreamcastSubNode.getPeripherals().size(), 0);
}

TEST_F(SubNodeTest, setConnectedConnectedToDisconnected)
{
    // --- MOCKING ---
    mDreamcastSubNode.mockSetConnected(true);
    // Add 2 mock peripherals to the sub node
    addTwoMockedPeripherals();

    // --- TEST EXECUTION ---
    mDreamcastSubNode.setConnected(false);

    // --- EXPECTATIONS ---
    EXPECT_FALSE(mDreamcastSubNode.isConnected());
    EXPECT_TRUE(mDreamcastSubNode.getPeripherals().empty());
}

TEST_F(SubNodeTest, setConnectedDisconnectedToDisconnected)
{
    // --- MOCKING ---
    mDreamcastSubNode.mockSetConnected(false);
    // Add 2 mock peripherals to the sub node
    addTwoMockedPeripherals();

    // --- TEST EXECUTION ---
    mDreamcastSubNode.setConnected(false);

    // --- EXPECTATIONS ---
    EXPECT_FALSE(mDreamcastSubNode.isConnected());
    EXPECT_EQ(mDreamcastSubNode.getPeripherals().size(), 2);
}
