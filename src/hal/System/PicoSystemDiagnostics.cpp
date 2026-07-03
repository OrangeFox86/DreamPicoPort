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

#include "PicoSystemDiagnostics.hpp"

#include "hardware/regs/addressmap.h"

#include <malloc.h>
#include <inttypes.h>

// Linker symbols defined by the Pico SDK memmap scripts
extern char __end__;        // Start of the heap pool (right after .bss)
extern char __HeapLimit;   // Hard limit of the heap pool (0x20040000)

PicoSystemDiagnostics::MemoryDiagnostics PicoSystemDiagnostics::getMemoryDiagnostics()
{
    MemoryDiagnostics md;

    uintptr_t heap_start_ptr = reinterpret_cast<uintptr_t>(&__end__);
    uintptr_t heap_ceil_ptr  = reinterpret_cast<uintptr_t>(&__HeapLimit);

    struct mallinfo mi = mallinfo();
    md.arena = mi.arena;
    md.ordblks = mi.ordblks;
    md.hblks = mi.hblks;
    md.hblkhd = mi.hblkhd;
    md.uordblks = mi.uordblks;
    md.fordblks = mi.fordblks;
    md.keepcost = mi.keepcost;
    md.heapstart = static_cast<std::size_t>(heap_start_ptr);
    md.heapceil = static_cast<std::size_t>(heap_ceil_ptr);

    return md;
}

bool PicoSystemDiagnostics::isInRam(const void* ptr)
{
    return (ptr >= (void*)SRAM_BASE);
}
