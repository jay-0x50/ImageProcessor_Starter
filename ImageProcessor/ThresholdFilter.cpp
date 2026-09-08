#include "ThresholdFilter.h"
#include "ColorUtils.h"
#include "Exceptions.h"

namespace ip {

ThresholdFilter::ThresholdFilter(int threshold) : m_threshold(threshold) {
    if (threshold < 0 || threshold > 255) {
        throw FilterError("Threshold must be between 0 and 255");
    }
}

void ThresholdFilter::applyTile(ImageBuffer& image, const Tile& tile) const {
    for (int y = tile.yBegin; y < tile.yEnd; ++y) {
        auto* pixel = image.rowPtr(y) + tile.xBegin * ImageBuffer::CHANNELS;
        for (int x = tile.xBegin; x < tile.xEnd; ++x, pixel += ImageBuffer::CHANNELS) {
            const auto value = static_cast<std::uint8_t>(luminance(pixel) >= m_threshold ? 255 : 0);
            pixel[0] = pixel[1] = pixel[2] = value;
        }
    }
}

} // namespace ip
