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

#include "EndpointTxScheduler.hpp"

EndpointTxScheduler::EndpointTxScheduler(
    const std::shared_ptr<PrioritizedTxScheduler>& prioritizedScheduler,
    uint8_t fixedPriority,
    uint8_t recipientAddr
):
    mPrioritizedScheduler(prioritizedScheduler),
    mFixedPriority(fixedPriority),
    mRecipientAddr(recipientAddr)
{}

EndpointTxScheduler::~EndpointTxScheduler()
{}

uint32_t EndpointTxScheduler::add(TransmissionProperties properties, Transmitter* transmitter)
{
    return mPrioritizedScheduler->add(
        properties.toSchedulerProperties(mFixedPriority, mRecipientAddr),
        transmitter
    );
}

uint32_t EndpointTxScheduler::add(TransmissionProperties properties, const std::shared_ptr<Transmitter>& transmitter)
{
    return mPrioritizedScheduler->add(
        properties.toSchedulerProperties(mFixedPriority, mRecipientAddr),
        transmitter
    );
}

uint32_t EndpointTxScheduler::cancelById(uint32_t transmissionId)
{
    return mPrioritizedScheduler->cancelById(transmissionId);
}

uint32_t EndpointTxScheduler::cancelByRecipient(uint8_t recipientAddr)
{
    return mPrioritizedScheduler->cancelByRecipient(recipientAddr);
}

uint32_t EndpointTxScheduler::countRecipients(uint8_t recipientAddr)
{
    return mPrioritizedScheduler->countRecipients(recipientAddr);
}

uint32_t EndpointTxScheduler::cancelAll()
{
    return mPrioritizedScheduler->cancelAll();
}