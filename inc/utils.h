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

#ifndef __UTILS_H__
#define __UTILS_H__

#include <algorithm>

#include "configuration.h"

#define INT_DIVIDE_CEILING(x,y) (((x) + (y) - 1) / (y))
#define INT_DIVIDE_ROUND(x,y) (((x) + ((y) / 2)) / (y))
// Just for completeness...
#define INT_DIVIDE_FLOOR(x,y) ((x)/(y))

#if SHOW_DEBUG_MESSAGES && !defined(UNITTEST)
    #include <stdio.h>
    #define DEBUG_PRINT(...) printf (__VA_ARGS__)
#else
    #define DEBUG_PRINT(...)
#endif

template <typename T>
inline T limit_value(T value, T min, T max)
{
    return std::min(std::max(value, min), max);
}

#endif // __UTILS_H__
