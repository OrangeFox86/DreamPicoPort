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

#include "SystemTtyCommandHandler.hpp"
#include "hal/MapleBus/MaplePacket.hpp"

#include <stdio.h>
#include <malloc.h>
#include <inttypes.h>

SystemTtyCommandHandler::SystemTtyCommandHandler(
    SystemIdentification& identification,
    ClockInterface& clock,
    const std::map<uint8_t, DreamcastNodeData>& dcNodes
) :
    mIdentification(identification),
    mClock(clock),
    mDcNodes(dcNodes)
{}

const char* SystemTtyCommandHandler::getCommandChars()
{
    return "-";
}


// The memory printout causes issues when compiled for unit testing purposes
#ifndef UNITTEST

// Linker symbols defined by the Pico SDK memmap scripts
extern char __end__;        // Start of the heap pool (right after .bss)
extern char __HeapLimit;   // Hard limit of the heap pool (0x20040000)

void print_memory_status(void)
{
    struct mallinfo mi = mallinfo();

    // TODO: should make another HAL component for this information

    printf("Non mmapped space (effective high watermark): %i\n", mi.arena);
    printf("Free chunks: %i\n", mi.ordblks);
    // printf("(always 0): %i\n", mi.smblks);
    printf("Num mmapped regions: %i\n", mi.hblks);
    printf("mmaped space: %i\n", mi.hblkhd);
    // printf("(always 0): %i\n", mi.usmblks);
    // printf("(always 0): %i\n", mi.fsmblks);
    printf("Total space: %i\n", mi.uordblks);
    printf("Free space: %i\n", mi.fordblks);
    printf("Releasable: %i\n", mi.keepcost);

    // This helps determine if running code is loaded into RAM or not
    const void* const fnPtr = (void*)print_memory_status;
    const bool isRam = (fnPtr >= (void*)0x20000000); // TODO: use SRAM_BASE (HAL needed)
    printf("Address of print_memory_status: 0x%p (%sin RAM)\n", fnPtr, isRam ? "" : "NOT ");

    // Cast to an integer type that matches the host's pointer size
    uintptr_t heap_start_ptr = reinterpret_cast<uintptr_t>(&__end__);
    uintptr_t heap_ceil_ptr  = reinterpret_cast<uintptr_t>(&__HeapLimit);

    // If you explicitly need them as 32-bit integers downstream for a protocol:
    uint32_t heap_start = static_cast<uint32_t>(heap_start_ptr);
    uint32_t heap_ceil  = static_cast<uint32_t>(heap_ceil_ptr);

    // 1. Total boundary allocated for the heap by the linker
    uint32_t total_heap_pool = heap_ceil - heap_start;

    // 2. High-water mark (how far the allocator has expanded up into the pool)
    uint32_t arena_expanded = mi.arena;

    // 3. Breakdown of that expanded arena
    uint32_t actively_used = mi.uordblks; // Memory currently held by your pointers
    uint32_t recycled_free = mi.fordblks; // Freed memory cached inside the allocator

    // 4. Calculations for true headroom
    uint32_t unexpanded_pool = total_heap_pool - arena_expanded;
    uint32_t actual_free_ram = unexpanded_pool + recycled_free;

    printf("\n==== RUNTIME HEAP PROFILE ====\n");
    printf("Heap Region:         0x%08" PRIX32 " - 0x%08" PRIX32 "\n", heap_start, heap_ceil);
    printf("Total Heap Pool:     %" PRIu32 " bytes\n", total_heap_pool);
    printf("  ├─ Unexpanded:     %" PRIu32 " bytes (Never touched yet)\n", unexpanded_pool);
    printf("  └─ Arena Expanded: %" PRIu32 " bytes (High-water mark)\n", arena_expanded);
    printf("      ├─ Actively Used: %" PRIu32 " bytes\n", actively_used);
    printf("      └─ Recycled Free: %" PRIu32 " bytes (Internal fragmentation)\n", recycled_free);
    printf("\nREAL AVAILABLE RAM: %" PRIu32 " bytes\n", actual_free_ram);
    printf("==============================\n");
}

#else

void print_memory_status(void) {}

#endif

void SystemTtyCommandHandler::submit(const char* chars, uint32_t len)
{
    if (len == 0)
    {
        // This shouldn't happen, but handle it regardless
        return;
    }

    printf("\n");

    const char* const eol = chars + len;
    const char* iter = chars + 1; // Skip past '-' (implied)

    if (iter == eol)
    {
        // Print system command help
        printf("-e: Echo text\n");
        printf("-S: Serial\n");
        printf("-$[0,3]: Maple bus status\n");
        printf("-m: Memory info\n");
        return;
    }

    const char cmd = *iter++;
    switch (cmd)
    {
        case 'e':
        {
            printf("%s\n", iter);
        }
        break;

        case 'S':
        {
            char buffer[mIdentification.getSerialSize() + 1] = {0};
            mIdentification.getSerial(buffer, sizeof(buffer) - 1);
            buffer[sizeof(buffer) - 1] = '\0';
            printf("%s\n", buffer);
        }
        break;

        case '$':
        {
            int idx = -1;
            if (iter < eol)
            {
                idx = *iter;
            }

            std::map<uint8_t, DreamcastNodeData>::iterator dcNodeIter = mDcNodes.end();
            if (idx >= 0 && (dcNodeIter = mDcNodes.find(idx)) != mDcNodes.end())
            {
                auto& node = *dcNodeIter->second.mainNode;

                std::string response;
                response.reserve(112);

                // Keep reading status until last two reads are equal or total of 3 reads made
                static constexpr uint32_t kMaxStatusReads = 3;
                DreamcastMainNode::MapleStatus status = node.getMapleStatus();
                bool synchronized = false;

                {
                    DreamcastMainNode::MapleStatus lastStatus;

                    for (uint32_t numStatusReads = 1; numStatusReads < kMaxStatusReads; ++numStatusReads)
                    {
                        lastStatus = status;
                        status = node.getMapleStatus();

                        if (status == lastStatus)
                        {
                            synchronized = true;
                            break;
                        }
                    }
                }

                const std::uint64_t now = mClock.getTimeUs();
                printf("Now: %" PRIu64 " us\n", now);
                printf("Data is %ssynchronized\n", synchronized ? "" : "NOT ");
                printf("Phase: %s\n", MapleBusInterface::phaseToString(status.phase));
                printf("Num Reads: %" PRIu64 "\n", status.mapleStats.numReads);

                printf("Num NULL Reads: %" PRIu64 "\n", status.mapleStats.numNullReads);
                printf("Num Read Fail CRC: %" PRIu64 "\n", status.mapleStats.numReadFailCrc);
                printf("Num Read Fail Incomplete: %" PRIu64 "\n", status.mapleStats.numReadFailIncomplete);
                printf("Num Read Fail Overflow: %" PRIu64 "\n", status.mapleStats.numReadFailOverflow);
                printf("Num Read Fail Timeout: %" PRIu64 "\n", status.mapleStats.numReadFailTimeout);
                printf("Last Read Start: %" PRIu64 " us\n", status.mapleStats.lastReadStartTime);
                printf("Last Read Complete: %" PRIu64 " us\n", status.mapleStats.lastReadCompleteTime);
                printf("Num Writes: %" PRIu64 "\n", status.mapleStats.numWrites);
                printf("Num Write Fail: %" PRIu64 "\n", status.mapleStats.numWriteFail);
                printf("Last Write Start: %" PRIu64 " us\n", status.mapleStats.lastWriteStartTime);
                printf("Last Write Complete: %" PRIu64 " us\n", status.mapleStats.lastWriteCompleteTime);
            }
            else
            {
                printf("Invalid index\n");
            }
        }
        break;

        case 'm':
        {
            print_memory_status();
        }
        break;

        default:
            printf("Invalid cmd: %c\n", cmd);
            break;
    }
}

void SystemTtyCommandHandler::printHelp()
{
    printf("-: System commands\n");
}
