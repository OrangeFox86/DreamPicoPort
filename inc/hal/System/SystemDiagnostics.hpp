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

 #include <cstddef>
#include <cstdint>

class SystemDiagnostics
{
    public:
        //! All memory diagnostics information
        struct MemoryDiagnostics
        {
            std::size_t arena; // total space allocated from system
            std::size_t ordblks; // number of non-inuse chunks
            std::size_t hblks; // number of mmapped regions
            std::size_t hblkhd; // total space in mmapped regions
            std::size_t uordblks; // total allocated space
            std::size_t fordblks; // total non-inuse space
            std::size_t keepcost; // top-most, releasable (via malloc_trim) space
            std::size_t totalram; // Total amount of RAM
            std::size_t heapstart; // Start of the heap pool (right after .bss)
            std::size_t heapceil; // Hard limit of the heap pool
        };

        virtual ~SystemDiagnostics() = default;

        //! @return current memory diagnostics data
        virtual MemoryDiagnostics getMemoryDiagnostics() = 0;

        //! Determines if a given pointer is in RAM
        //! @param[in] ptr The pointer to check
        //! @return true iff the given pointer is in RAM
        virtual bool isInRam(const void* ptr) = 0;
};
