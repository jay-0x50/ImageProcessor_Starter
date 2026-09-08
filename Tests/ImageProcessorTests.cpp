#include "BmpParser.h"
#include "Exceptions.h"
#include "FilterBase.h"
#include "GrayscaleFilter.h"
#include "ThresholdFilter.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename Error, typename Function>
void expectError(Function function, const std::string& message) {
    try {
        function();
    }
    catch (const Error&) {
        return;
    }
    throw std::runtime_error("Expected exception: " + message);
}

bool equal(const ip::ImageBuffer& left, const ip::ImageBuffer& right) {
    return left.width() == right.width() && left.height() == right.height() &&
        std::equal(left.data(), left.data() + left.dataSize(), right.data());
}

void writeBytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    file.close();
    require(static_cast<bool>(file), "Fixture write failed");
}

std::vector<std::uint8_t> readBytes(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    require(static_cast<bool>(file), "Fixture read failed");
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void put32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned int i = 0; i < 4; ++i) {
        bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8));
    }
}

// 3x2 BMP를 파서에 의존하지 않고 직접 구성한다. 행당 9바이트 + 패딩 3바이트.
std::vector<std::uint8_t> makeBmp(bool topDown) {
    std::vector<std::uint8_t> bytes(78, 0);
    bytes[0] = 'B'; bytes[1] = 'M';
    put32(bytes, 2, 78);
    put32(bytes, 10, 54);
    put32(bytes, 14, 40);
    put32(bytes, 18, 3);
    put32(bytes, 22, topDown ? static_cast<std::uint32_t>(-2) : 2u);
    bytes[26] = 1;
    bytes[28] = 24;
    put32(bytes, 34, 24);
    const std::uint8_t pixels[18] = {
        0, 0, 255, 0, 255, 0, 255, 0, 0,
        0, 0, 0, 128, 128, 128, 255, 255, 255
    };
    for (int row = 0; row < 2; ++row) {
        const int y = topDown ? row : 1 - row;
        std::copy_n(pixels + y * 9, 9, bytes.begin() + 54 + row * 12);
        std::fill_n(bytes.begin() + 54 + row * 12 + 9, 3, static_cast<std::uint8_t>(0xAB));
    }
    return bytes;
}

void testPixels() {
    ip::ImageBuffer original(8, 1);
    const std::uint8_t pixels[] = {
        0, 0, 255, 0, 255, 0, 255, 0, 0, 0, 0, 0,
        127, 127, 127, 128, 128, 128, 254, 254, 254, 255, 255, 255
    };
    std::copy(std::begin(pixels), std::end(pixels), original.data());
    auto gray = original;
    ip::GrayscaleFilter().apply(gray, 1);
    const int expected[] = {76, 149, 29, 0, 127, 128, 254, 255};
    for (int x = 0; x < 8; ++x) {
        for (int channel = 0; channel < 3; ++channel) {
            require(gray.data()[x * 3 + channel] == expected[x], "Grayscale known color mismatch");
        }
    }
    for (const int threshold : {0, 128, 255}) {
        auto binary = original;
        ip::ThresholdFilter(threshold).apply(binary, 4);
        for (int x = 0; x < 8; ++x) {
            for (int channel = 0; channel < 3; ++channel) {
                require(binary.data()[x * 3 + channel] == (expected[x] >= threshold ? 255 : 0),
                        "Threshold boundary mismatch");
            }
        }
    }
}

class CoverageFilter final : public ip::FilterBase {
    void applyTile(ip::ImageBuffer& image, const Tile& tile) const override {
        for (int y = tile.yBegin; y < tile.yEnd; ++y) {
            for (int x = tile.xBegin; x < tile.xEnd; ++x) {
                ++image.rowPtr(y)[x * 3];
            }
        }
    }
};

class ThrowingFilter final : public ip::FilterBase {
    void applyTile(ip::ImageBuffer&, const Tile&) const override {
        throw std::runtime_error("worker failure");
    }
};

void testParallel() {
    for (const auto& dimensions : {std::pair<int, int>{1, 1}, {1, 259}, {259, 1}, {259, 257}}) {
        ip::ImageBuffer source(dimensions.first, dimensions.second);
        for (std::size_t i = 0; i < source.dataSize(); ++i) {
            source.data()[i] = static_cast<std::uint8_t>((i * 37 + i / 11) % 256);
        }
        for (const unsigned int workers : {1u, 2u, 4u, 256u, 0u}) {
            auto serial = source;
            auto parallel = source;
            ip::GrayscaleFilter().apply(serial, 1);
            ip::GrayscaleFilter().apply(parallel, workers);
            require(equal(serial, parallel), "Parallel grayscale mismatch");
            serial = source; parallel = source;
            ip::ThresholdFilter(128).apply(serial, 1);
            ip::ThresholdFilter(128).apply(parallel, workers);
            require(equal(serial, parallel), "Parallel threshold mismatch");
        }
    }
    ip::ImageBuffer coverage(259, 257);
    require(CoverageFilter().apply(coverage, 4) == 4, "Expected four workers");
    for (std::size_t i = 0; i < coverage.dataSize(); i += 3) {
        require(coverage.data()[i] == 1, "Tile must cover each pixel exactly once");
    }
    expectError<ip::FilterError>([&] { ThrowingFilter().apply(coverage, 4); }, "worker propagation");
    ip::ImageBuffer tiny(1, 1);
    require(ip::GrayscaleFilter().apply(tiny, 256) == 1, "Limit workers to available tiles");
    ip::ImageBuffer empty;
    expectError<ip::FilterError>([&] { ip::GrayscaleFilter().apply(empty); }, "empty image");
    expectError<ip::FilterError>([&] { ip::GrayscaleFilter().apply(tiny, 257); }, "thread limit");
    expectError<ip::FilterError>([] { ip::ThresholdFilter invalid(-1); }, "negative threshold");
    expectError<ip::FilterError>([] { ip::ThresholdFilter invalid(256); }, "large threshold");
    expectError<std::invalid_argument>([] { ip::ImageBuffer invalid(0, 1); }, "zero width");
    expectError<std::invalid_argument>([] { ip::ImageBuffer invalid(65536, 65536); }, "pixel overflow");
}

void testBmp(const std::filesystem::path& directory) {
    const auto fixture = directory / "fixture.bmp";
    const auto saved = directory / "saved.bmp";
    ip::ImageBuffer reference;
    for (const bool topDown : {false, true}) {
        writeBytes(fixture, makeBmp(topDown));
        auto image = ip::BmpParser::loadFromFile(fixture.string());
        require(image.width() == 3 && image.height() == 2, "BMP dimensions");
        require(image.rowPtr(0)[2] == 255 && image.rowPtr(1)[3] == 128, "BMP row orientation");
        if (!reference.empty()) require(equal(reference, image), "BMP orientation mismatch");
        reference = image;
        ip::BmpParser::saveToFile(saved.string(), image);
        require(equal(image, ip::BmpParser::loadFromFile(saved.string())), "BMP round trip");
        const auto bytes = readBytes(saved);
        require(bytes.size() == 78 && bytes[22] == 2 && bytes[54] == 0 && bytes[66 + 2] == 255,
                "Saved BMP layout");
        for (const int offset : {63, 64, 65, 75, 76, 77}) require(bytes[offset] == 0, "Zero padding");
    }
    const auto reject = [&](std::vector<std::uint8_t> bytes) {
        writeBytes(fixture, bytes);
        expectError<ip::BmpParseError>([&] { ip::BmpParser::loadFromFile(fixture.string()); }, "bad BMP");
    };
    auto bytes = makeBmp(false); bytes[0] = 'X'; reject(bytes);
    bytes = makeBmp(false); bytes.resize(20); reject(bytes);
    bytes = makeBmp(false); bytes.pop_back(); reject(bytes);
    bytes = makeBmp(false); bytes[28] = 32; reject(bytes);
    bytes = makeBmp(false); bytes[30] = 1; reject(bytes);
    bytes = makeBmp(false); bytes[26] = 0; reject(bytes);
    bytes = makeBmp(false); put32(bytes, 14, 12); reject(bytes);
    bytes = makeBmp(false); put32(bytes, 10, 0); reject(bytes);
    bytes = makeBmp(false); put32(bytes, 18, 0); reject(bytes);
    bytes = makeBmp(false); put32(bytes, 22, 0x80000000u); reject(bytes);
    bytes = makeBmp(false); put32(bytes, 18, 0x7FFFFFFFu); reject(bytes);
    bytes = makeBmp(false); put32(bytes, 2, 54); reject(bytes);
    expectError<ip::BmpParseError>([&] {
        ip::BmpParser::loadFromFile((directory / "missing.bmp").string());
    }, "missing input");
    expectError<ip::BmpParseError>([&] {
        ip::BmpParser::saveToFile(directory.string(), reference);
    }, "unwritable output");
}

// 성능은 환경 의존적이므로 합격 조건으로 삼지 않고 Release 측정값을 출력한다.
void benchmark(const char* name, const ip::FilterBase& filter) {
    ip::ImageBuffer source(4096, 4096);
    for (std::size_t i = 0; i < source.dataSize(); ++i) {
        source.data()[i] = static_cast<std::uint8_t>(i % 256);
    }
    std::vector<double> serialTimes;
    std::vector<double> parallelTimes;
    for (int iteration = 0; iteration < 8; ++iteration) {
        for (const unsigned int workers : {1u, 4u}) {
            auto image = source; // 복사와 BMP 입출력 시간은 측정에서 제외한다.
            const auto start = std::chrono::steady_clock::now();
            filter.apply(image, workers);
            const double elapsed = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - start).count();
            if (iteration > 0) {
                (workers == 1 ? serialTimes : parallelTimes).push_back(elapsed);
            }
        }
    }
    std::sort(serialTimes.begin(), serialTimes.end());
    std::sort(parallelTimes.begin(), parallelTimes.end());
    std::cout << "BENCH " << name << " 4096x4096, median of 7: one=" << serialTimes[3]
              << " ms, four=" << parallelTimes[3] << " ms, speedup="
              << serialTimes[3] / parallelTimes[3] << "x\n";
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        require(argc == 2, "Pass the test artifact directory");
        const std::filesystem::path directory(argv[1]);
        std::filesystem::create_directories(directory);
        testPixels();
        testParallel();
        testBmp(directory);
        std::cout << "PASS: pixel values, threshold boundaries, tile coverage, worker exceptions, BMP validation\n";
        benchmark("grayscale", ip::GrayscaleFilter());
        benchmark("threshold", ip::ThresholdFilter());
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
