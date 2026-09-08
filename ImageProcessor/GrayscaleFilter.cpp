#include "GrayscaleFilter.h"
#include "ColorUtils.h"

namespace ip {

void GrayscaleFilter::applyTile(ImageBuffer& image, const Tile& tile) const {
    for (int y = tile.yBegin; y < tile.yEnd; ++y) {
        auto* pixel = image.rowPtr(y) + tile.xBegin * ImageBuffer::CHANNELS;
        for (int x = tile.xBegin; x < tile.xEnd; ++x, pixel += ImageBuffer::CHANNELS) {
            const auto gray = luminance(pixel);
            pixel[0] = pixel[1] = pixel[2] = gray;
        }
    }
}

} // namespace ip
