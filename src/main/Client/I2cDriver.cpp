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

#include "I2cDriver.hpp"

#include "hardware/gpio.h"

i2c_inst_t* const I2cDriver::I2C_INSTANCES[2] = {i2c0, i2c1};

I2cDriver::I2cDriver(uint32_t i2cIdx, uint32_t baud, uint32_t gpioStart) :
    mI2c(I2cDriver::I2C_INSTANCES[i2cIdx]),
    mDeviceList(),
    mProcAddr(0),
    mProcDevice(nullptr),
    mProcState(State::NONE),
    mWriteComplete(false),
    mNumRead(0),
    mNumReadWaiting(0),
    mFirstByte(false)
{
    i2c_init(mI2c, baud);
    gpio_set_function(gpioStart, GPIO_FUNC_I2C);
    gpio_set_function(gpioStart + 1, GPIO_FUNC_I2C);
    gpio_pull_up(gpioStart);
    gpio_pull_up(gpioStart + 1);
}

void I2cDriver::addDevice(I2cDevice* device)
{
    mDeviceList.push_back(device);
}

void I2cDriver::start(std::list<I2cDevice*>::iterator iter)
{
    mProcDevice = *iter;
    mProcAddr = mProcDevice->getAddress();
    mFirstByte = true;
    (void)mI2c->hw->clr_tx_abrt;
}

void I2cDriver::end()
{
    if (mProcDevice != nullptr)
    {
        // Move the last processed device to the end of the list
        mDeviceList.remove(mProcDevice);
        mDeviceList.push_back(mProcDevice);
    }

    mProcState = State::NONE;
    mProcDevice = nullptr;
}

bool I2cDriver::process()
{
    bool processing = false;

    // TODO: it may be a good idea to add a timeout if we keep looping without activity

    if (mProcState == State::NONE || mProcDevice == nullptr)
    {
        if (mI2c->hw->status & I2C_IC_STATUS_ACTIVITY_BITS)
        {
            // I2C still active; wait for idle
            return true;
            // TODO: this may mean that a fault occurred, and stop condition needs to be sent or HW disabled
        }

        // See if there are any I2C devices that are ready to receive or transmit
        // Priority is just order of map - if this becomes a problem, the map can be reordered
        // every time a target is selected
        for (std::list<I2cDevice*>::iterator iter = mDeviceList.begin();
            iter != mDeviceList.end();
            ++iter)
        {
            I2cDevice* dev = *iter;

            if (dev->isOutAvailable())
            {
                mWriteComplete = false;
                mProcState = State::WRITING;
                start(iter);
                break; // break out of for loop
            }
            else if ((mNumRead = dev->isInExpected()) > 0)
            {
                mNumReadWaiting = mNumRead;
                mProcState = State::READING;
                start(iter);
                break; // break out of for loop
            }
        }
    }

    if (mProcState != State::NONE)
    {
        processing = true;
        if (mI2c->hw->tar != mProcAddr)
        {
            // See if the hardware is still finishing up previous data
            if (mI2c->hw->txflr > 0)
            {
                // Keep waiting
                return processing;
            }

            while (mI2c->hw->rxflr > 0)
            {
                uint8_t dat = mI2c->hw->data_cmd;
                (void)dat;
            }
        }

        if (mFirstByte)
        {
            mI2c->hw->enable = 0;
            mI2c->hw->tar = mProcAddr;
            mI2c->hw->enable = 1;
            mI2c->restart_on_next = false;
        }

        switch (mProcState)
        {
            case State::WRITING:
            {
                while(mI2c->hw->txflr < 16 && !mWriteComplete)
                {
                    uint8_t byte = 0;
                    bool sendStop = false;
                    bool anotherByte = mProcDevice->nextByteOut(byte, sendStop);

                    uint32_t cmd =
                        bool_to_bit(mFirstByte || mI2c->restart_on_next) << I2C_IC_DATA_CMD_RESTART_LSB |
                        bool_to_bit(!anotherByte || sendStop) << I2C_IC_DATA_CMD_STOP_LSB |
                        byte;

                    mI2c->hw->data_cmd = cmd;

                    mFirstByte = false;
                    mI2c->restart_on_next = (anotherByte ? sendStop : false);

                    mWriteComplete = !anotherByte;

                    if (checkForAbort())
                    {
                        // Aborted
                        return processing;
                    }
                }

                if (mWriteComplete)
                {
                    if (checkForAbort())
                    {
                        // Aborted
                        return processing;
                    }

                    if (mI2c->hw->txflr > 0)
                    {
                        // Still waiting for write to flush
                        return processing;
                    }

                    // Complete
                    end();
                    return processing;
                }
            }
            break;

            case State::READING:
            {
                // Read data off of RX FIFO
                while(mI2c->hw->rxflr > 0 && mNumReadWaiting > 0)
                {
                    uint8_t dat = mI2c->hw->data_cmd;
                    mProcDevice->nextByteIn(dat);
                    --mNumReadWaiting;
                }

                if (mNumReadWaiting == 0)
                {
                    // Complete
                    end();
                    return processing;
                }

                if (checkForAbort())
                {
                    // Aborted
                    return processing;
                }

                // Queue read on TX FIFO
                while(mI2c->hw->txflr < 16 && mNumRead > 0)
                {
                    bool last = (--mNumRead == 0);

                    mI2c->hw->data_cmd =
                        bool_to_bit(mFirstByte) << I2C_IC_DATA_CMD_RESTART_LSB |
                        bool_to_bit(last) << I2C_IC_DATA_CMD_STOP_LSB |
                        I2C_IC_DATA_CMD_CMD_BITS; // -> 1 for read

                    mFirstByte = false;

                    if (checkForAbort())
                    {
                        // Aborted
                        return processing;
                    }
                }
            }
            break;

            default: break;
        }
    }

    return processing;
}

void I2cDriver::flush(int32_t timeoutMs)
{
    uint64_t timeoutTimeUs = 0;
    if (timeoutMs >= 0)
    {
        timeoutTimeUs = time_us_64() + (timeoutMs * 1000);
    }

    while(process() && (timeoutMs < 0 || time_us_64() < timeoutTimeUs));
}

bool I2cDriver::checkForAbort()
{
    uint32_t abortReason = mI2c->hw->tx_abrt_source;

    if (abortReason != 0)
    {
        // I2C write or read failed
        // Clear failure by reading this register
        mI2c->hw->clr_tx_abrt;

        switch (mProcState)
        {
            case State::WRITING:
                mProcDevice->writeFailed(abortReason);
                break;

            case State::READING:
                mProcDevice->readFailed(abortReason);
                break;

            default: break;
        }

        // Go back to idle
        end();

        return true;
    }

    return false;
}
