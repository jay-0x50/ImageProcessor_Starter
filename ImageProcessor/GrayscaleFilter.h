#pragma once

#include "FilterBase.h"

namespace ip {

class GrayscaleFilter final : public FilterBase {
protected:
    void applyTile(ImageBuffer& image, const Tile& tile) const override;
};

} // namespace ip
