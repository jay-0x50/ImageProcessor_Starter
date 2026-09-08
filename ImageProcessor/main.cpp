#include "BmpParser.h"
#include "CommandLineParser.h"
#include "Exceptions.h"
#include "FilterBase.h"
#include "GrayscaleFilter.h"
#include "Logger.h"
#include "ThresholdFilter.h"

#include <chrono>
#include <iostream>
#include <memory>

namespace {

using Clock = std::chrono::steady_clock;

double millisecondsSince(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

// 새 필터는 생성 분기에 등록한다. BMP 입출력과 공통 실행 코드는 바뀌지 않는다.
std::unique_ptr<ip::FilterBase> createFilter(const ip::ProgramOptions& options) {
    if (options.filterName == "grayscale") return std::make_unique<ip::GrayscaleFilter>();
    if (options.filterName == "threshold") return std::make_unique<ip::ThresholdFilter>(options.threshold);
    throw ip::FilterError("Unknown filter: " + options.filterName);
}

} // namespace

int main(int argc, char* argv[]) {
    const auto start = Clock::now();
    ip::ProgramOptions options;
    ip::RunLog record;
    ip::Logger logger;
    int exitCode = 0;

    try {
        options = ip::CommandLineParser::parse(argc, argv);
        if (options.showHelp) {
            ip::CommandLineParser::printUsage(argc > 0 ? argv[0] : "ImageProcessor");
            return 0;
        }

        logger.open(options);
        const auto filter = createFilter(options);
        ip::ImageBuffer image = ip::BmpParser::loadFromFile(options.inputPath);
        std::cout << "Loaded: " << image.width() << " x " << image.height() << '\n';

        const auto filterStart = Clock::now();
        try {
            record.actualThreads = filter->apply(image, options.threadCount);
        }
        catch (...) {
            record.filterMs = millisecondsSince(filterStart);
            throw;
        }
        record.filterMs = millisecondsSince(filterStart);

        ip::BmpParser::saveToFile(options.outputPath, image);
        std::cout << "Saved: " << options.outputPath << '\n'
                  << "Filter: " << options.filterName << ", threads: " << record.actualThreads
                  << ", filter time: " << record.filterMs << " ms\n";
        record.success = true;
        record.message = "Completed";
    }
    catch (const ip::ArgumentError& error) {
        record.message = error.what();
        exitCode = 4;
        ip::CommandLineParser::printUsage(argc > 0 ? argv[0] : "ImageProcessor");
    }
    catch (const ip::BmpParseError& error) {
        record.message = error.what();
        exitCode = 2;
    }
    catch (const ip::FilterError& error) {
        record.message = error.what();
        exitCode = 3;
    }
    catch (const std::exception& error) {
        record.message = error.what();
        exitCode = 1;
    }
    catch (...) {
        record.message = "Unknown error";
        exitCode = 1;
    }

    record.elapsedMs = millisecondsSince(start);
    if (exitCode != 0) std::cerr << record.message << '\n';

    try {
        // 인자 검증을 통과해 안전하게 열린 로그 파일에만 기록한다.
        if (logger.isOpen()) logger.write(options, record);
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        if (exitCode == 0) exitCode = 1;
    }
    return exitCode;
}
