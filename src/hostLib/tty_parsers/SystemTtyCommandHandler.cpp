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
    SystemDiagnostics& diagnostics,
    ClockInterface& clock,
    const std::map<uint8_t, DreamcastNodeData>& dcNodes
) :
    mIdentification(identification),
    mDiagnostics(diagnostics),
    mClock(clock),
    mDcNodes(dcNodes)
{}

const char* SystemTtyCommandHandler::getCommandChars()
{
    return "-";
}

void print_memory_status(SystemDiagnostics& diagnostics)
{
    // This helps determine if running code is loaded into RAM or not
    const void* const fnPtr = (void*)print_memory_status;
    const bool isRam = diagnostics.isInRam(fnPtr);
    printf("Code currently executing within %s\n", isRam ? "RAM" : "Flash");

    SystemDiagnostics::MemoryDiagnostics md = diagnostics.getMemoryDiagnostics();

    printf("Heap non mmapped space: %zu\n", md.arena);
    printf("Heap free chunks: %zu\n", md.ordblks);
    printf("Heap num mmapped regions: %zu\n", md.hblks);
    printf("Heap mmaped space: %zu\n", md.hblkhd);
    printf("Heap total space: %zu\n", md.uordblks);
    printf("Heap free space: %zu\n", md.fordblks);
    printf("Heap releasable: %zu\n", md.keepcost);

    // 1. Total boundary allocated for the heap by the linker
    uint32_t total_heap_pool = static_cast<uint32_t>(md.heapceil - md.heapstart);

    // 2. High-water mark (how far the allocator has expanded up into the pool)
    uint32_t arena_expanded = static_cast<uint32_t>(md.arena);

    // 3. Breakdown of that expanded arena
    uint32_t actively_used = static_cast<uint32_t>(md.uordblks); // Memory currently held by application's pointers
    uint32_t recycled_free = static_cast<uint32_t>(md.fordblks); // Freed memory cached inside the allocator

    // 4. Calculations for true headroom
    uint32_t unexpanded_pool = total_heap_pool - arena_expanded;
    uint32_t actual_free_ram = unexpanded_pool + recycled_free;

    printf("\n==== RUNTIME HEAP PROFILE ====\n");
    printf(
        "Heap Region:         0x%08" PRIX32 " - 0x%08" PRIX32 "\n",
        static_cast<uint32_t>(md.heapstart),
        static_cast<uint32_t>(md.heapceil)
    );
    printf("Total Heap Pool:     %" PRIu32 " bytes\n", total_heap_pool);
    printf("  ├─ Unexpanded:     %" PRIu32 " bytes (Never touched yet)\n", unexpanded_pool);
    printf("  └─ Arena Expanded: %" PRIu32 " bytes (High-water mark)\n", arena_expanded);
    printf("      ├─ Actively Used: %" PRIu32 " bytes\n", actively_used);
    printf("      └─ Recycled Free: %" PRIu32 " bytes (Internal fragmentation)\n", recycled_free);
    printf("\nREAL AVAILABLE RAM: %" PRIu32 " bytes\n", actual_free_ram);
    printf("==============================\n");
}

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
            if (iter < eol && *iter >= '0' && *iter <= '9')
            {
                idx = *iter - '0';
            }

            std::map<uint8_t, DreamcastNodeData>::iterator dcNodeIter = mDcNodes.end();
            if (idx >= 0 && (dcNodeIter = mDcNodes.find(idx)) != mDcNodes.end())
            {
                auto& node = *dcNodeIter->second.mainNode;

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
            print_memory_status(mDiagnostics);
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
