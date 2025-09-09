// Copyright (C) 2025
//
// Based on reverse engineering of Liar-soft LIM format
// Ported from LimDecoder.cs (C#) into arc_unpacker component
//
// This file is part of arc_unpacker.
//
// arc_unpacker is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// arc_unpacker is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// General Public License for more details.

#include "dec/liar_soft/lim_image_decoder.h"
#include "algo/format.h"
#include "algo/range.h"
#include "dec/liar_soft/cg_decompress.h"
#include "err.h"

using namespace au;
using namespace au::dec::liar_soft;

static const bstr magic = "LM"_b;

bool LimImageDecoder::is_recognized_impl(io::File &input_file) const
{
    if (input_file.stream.read(magic.size()) != magic)
        return false;
    const auto flags = input_file.stream.read_le<u16>();
    return (flags & 0xF) == 2 || (flags & 0xF) == 3;
}

res::Image LimImageDecoder::decode_impl(
    const Logger &logger, io::File &input_file) const
{
    // --- header ---
    input_file.stream.seek(magic.size());
    const auto flags = input_file.stream.read_le<u16>();
    const auto bpp_field = input_file.stream.read_le<u16>(); // 原来的depth_field实际是bpp
    const auto skip = input_file.stream.read_le<u16>();      // 新增：关键的skip字段
    const auto width = input_file.stream.read_le<u32>();
    const auto height = input_file.stream.read_le<u32>();
    const auto canvas_size = width * height;
    u16 depth;
    if (bpp_field == 0x10)
        depth = 16;
    else
        depth = 32;


    if (depth == 32)
    {
        // 32-bit BGRA, 4 channels separately compressed
        bstr raw(canvas_size * 4, 0);
        u8 mask = 0xFF;
        for (int ch = 3; ch >= 0; --ch)
        {
            bstr channel(canvas_size);
            cg_decompress(channel, 0, 1, input_file.stream, 1);
            size_t src = 0;
            for (size_t i = ch; i < raw.size(); i += 4)
                raw[i] = channel[src++] ^ mask;
            mask = 0;
        }

        // swap BGRA -> RGBA
        for (size_t i = 0; i < raw.size(); i += 4)
            std::swap(raw[i], raw[i + 2]);

        return res::Image(width, height, raw, res::PixelFormat::RGBA8888);
    }

    else // depth == 16
    {
        bstr raw(canvas_size * 2);
        if (flags & 0x10)
        {
            if (flags & 0xE0)
                cg_decompress(raw, 0, 2, input_file.stream, 2);
            else
                raw = input_file.stream.read(canvas_size * 2);
        }

        res::Image image(width, height, raw, res::PixelFormat::BGR565);

        // --- optional alpha ---
        if (flags & 0x100)
        {
            bstr alpha;
            if (flags & 0xE00)
            {
                alpha.resize(canvas_size);
                cg_decompress(alpha, 0, 1, input_file.stream, 1);
            }
            else
                alpha = input_file.stream.read(canvas_size);

            for (auto &c : alpha)
                c = ~c;

            res::Image mask(width, height, alpha, res::PixelFormat::Gray8);
            image.apply_mask(mask);
        }

        return image;
    }
}

static auto _ = dec::register_decoder<LimImageDecoder>("liar-soft/lim");
