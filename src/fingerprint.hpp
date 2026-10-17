#pragma once
#include <cstddef>
#include <cstdint>

// Ignore GPU row padding; match only the known uncompressed UI tiles.
inline std::uint64_t pixelFingerprint(const void* pixels, std::size_t pitch,
                                     unsigned width, unsigned height) noexcept
{
    if (!pixels || !width || !height || width > 256 || height > 256 || pitch < width * 4)
        return 0;
    auto bytes = static_cast<const std::uint8_t*>(pixels);
    std::uint64_t hash = 14695981039346656037ull;
    for (unsigned y = 0; y < height; ++y)
        for (unsigned x = 0; x < width * 4; ++x) {
            hash ^= bytes[y * pitch + x];
            hash *= 1099511628211ull;
        }
    return hash;
}
