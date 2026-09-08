#pragma once

#include "ImageBuffer.h"

namespace ip {

// 기반 클래스는 타일 분할/스레드 수명을, 파생 클래스는 픽셀 연산을 담당한다.
class FilterBase {
public:
    virtual ~FilterBase() = default;

    // 0: 하드웨어 동시 실행 수 사용. 실제 작업 스레드 수(호출 스레드 포함)를 반환한다.
    unsigned int apply(ImageBuffer& image, unsigned int threadCount = 0) const;

protected:
    struct Tile {
        int xBegin;
        int yBegin;
        int xEnd;
        int yEnd;
    };

    // 동시에 호출되므로 지정 영역만 수정하고 필터 상태는 변경하지 않는다.
    virtual void applyTile(ImageBuffer& image, const Tile& tile) const = 0;
};

} // namespace ip
