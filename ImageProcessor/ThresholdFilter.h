#pragma once

#include "FilterBase.h"

namespace ip {

class ThresholdFilter final : public FilterBase {
public:
    explicit ThresholdFilter(int threshold = 128);

protected:
    void applyTile(ImageBuffer& image, const Tile& tile) const override;

private:
    const int m_threshold;
};

} // namespace ip
