#include "FilterBase.h"
#include "Exceptions.h"

#include <algorithm>
#include <exception>
#include <thread>
#include <vector>

namespace ip {
namespace {

constexpr int TILE_SIZE = 128;
constexpr unsigned int MAX_THREADS = 256;

// 스레드 생성 도중 예외가 발생해도 이미 시작한 작업을 모두 기다린다.
class ThreadJoiner {
public:
    explicit ThreadJoiner(std::vector<std::thread>& threads) : m_threads(threads) {}
    ~ThreadJoiner() {
        for (auto& thread : m_threads) {
            if (thread.joinable()) thread.join();
        }
    }
    ThreadJoiner(const ThreadJoiner&) = delete;
    ThreadJoiner& operator=(const ThreadJoiner&) = delete;

private:
    std::vector<std::thread>& m_threads;
};

} // namespace

unsigned int FilterBase::apply(ImageBuffer& image, unsigned int threadCount) const {
    if (image.empty()) throw FilterError("Cannot process an empty image");
    if (threadCount > MAX_THREADS) throw FilterError("Thread count must not exceed 256");
    if (threadCount == 0) {
        threadCount = std::clamp(std::thread::hardware_concurrency(), 1u, MAX_THREADS);
    }

    const int columns = (image.width() - 1) / TILE_SIZE + 1;
    const int rows = (image.height() - 1) / TILE_SIZE + 1;
    const auto tileCount = static_cast<std::size_t>(columns) * rows;
    threadCount = static_cast<unsigned int>(std::min<std::size_t>(threadCount, tileCount));

    const auto processTiles = [&](unsigned int worker) {
        for (std::size_t index = worker; index < tileCount; index += threadCount) {
            const int x = static_cast<int>(index % columns) * TILE_SIZE;
            const int y = static_cast<int>(index / columns) * TILE_SIZE;
            applyTile(image, {x, y, std::min(x + TILE_SIZE, image.width()),
                                  std::min(y + TILE_SIZE, image.height())});
        }
    };

    try {
        if (threadCount == 1) {
            processTiles(0);
            return 1;
        }

        // 작업마다 독립적인 예외 슬롯과 겹치지 않는 픽셀 영역을 사용한다.
        std::vector<std::exception_ptr> errors(threadCount);
        const auto runWorker = [&](unsigned int worker) {
            try {
                processTiles(worker);
            }
            catch (...) {
                errors[worker] = std::current_exception();
            }
        };

        std::vector<std::thread> threads;
        threads.reserve(threadCount - 1);
        {
            ThreadJoiner joiner(threads);
            for (unsigned int worker = 1; worker < threadCount; ++worker) {
                threads.emplace_back(runWorker, worker);
            }
            runWorker(0);
        }
        for (const auto& error : errors) {
            if (error) std::rethrow_exception(error);
        }
    }
    catch (const FilterError&) {
        throw;
    }
    catch (const std::exception& error) {
        throw FilterError(error.what());
    }
    catch (...) {
        throw FilterError("Unknown exception during tile processing");
    }
    return threadCount;
}

} // namespace ip
