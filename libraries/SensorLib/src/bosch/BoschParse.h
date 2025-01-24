/**
 *
 * @license MIT License
 *
 * Copyright (c) 2022 lewis he
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * @file      BoschParse.h
 * @author    Lewis He (lewishe@outlook.com)
 * @date      2023-09-30
 *
 */
#pragma once

#include "SensorCommon.tpp"
#include "SensorBhy2Define.h"
#include "bosch/bhy2_parse.h"
#include "bosch/common/common.h"

#if defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_STM32)
#include <vector>
#include <functional>
#define USE_STD_VECTOR
#endif


enum BoschOrientation {
    BHY2_DIRECTION_TOP_RIGHT,
    BHY2_DIRECTION_TOP_LEFT,
    BHY2_DIRECTION_BOTTOM_LEFT,
    BHY2_DIRECTION_BOTTOM_RIGHT,
};

class BoschParse
{
public:
#ifdef USE_STD_VECTOR
    static std::vector<ParseCallBackList_t> bhyParseEventVector;
#else
    static ParseCallBackList_t *BoschParse_bhyParseEventVector;
    static uint32_t BoschParse_bhyParseEventVectorSize;
    static uint32_t BoschParse_bhyParseEventVectorCapacity ;
    static void expandParseEventVector();
#endif
    static BhyEventCb _event_callback;
    static BhyDebugMessageCallback _debug_callback;

    static void parseData(const struct bhy2_fifo_parse_data_info *fifo, void *user_data);

    static void parseMetaEvent(const struct bhy2_fifo_parse_data_info *callback_info, void *user_data);

    static void parseDebugMessage(const struct bhy2_fifo_parse_data_info *callback_info, void *callback_ref);

};
