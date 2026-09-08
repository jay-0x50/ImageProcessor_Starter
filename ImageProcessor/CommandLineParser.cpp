#include "CommandLineParser.h"
#include "Exceptions.h"

#include <charconv>
#include <iostream>
#include <set>
#include <string>
#include <system_error>

namespace ip {
namespace {

std::string nextArg(int argc, char* argv[], int& i, const std::string& flag) {
    if (i + 1 >= argc || std::string(argv[i + 1]).empty() ||
        std::string(argv[i + 1]).rfind("--", 0) == 0) {
        throw ArgumentError(flag + ": missing value");
    }
    return argv[++i];
}

int parseNumber(const std::string& value, int minimum, int maximum, const std::string& flag) {
    int number = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), number);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size() ||
        number < minimum || number > maximum) {
        throw ArgumentError(flag + " must be an integer in [" +
            std::to_string(minimum) + ", " + std::to_string(maximum) + "]");
    }
    return number;
}

} // namespace

ProgramOptions CommandLineParser::parse(int argc, char* argv[]) {
    ProgramOptions options;
    std::set<std::string> seen;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-i") arg = "--input";
        else if (arg == "-o") arg = "--output";
        else if (arg == "-f") arg = "--filter";
        else if (arg == "-h") arg = "--help";

        if (!seen.insert(arg).second) {
            throw ArgumentError("Duplicate option: " + arg);
        }
        if (arg == "--help") {
            options.showHelp = true;
            return options;
        }
        if (arg == "--input") {
            options.inputPath = nextArg(argc, argv, i, arg);
        }
        else if (arg == "--output") {
            options.outputPath = nextArg(argc, argv, i, arg);
        }
        else if (arg == "--filter") {
            options.filterName = nextArg(argc, argv, i, arg);
        }
        else if (arg == "--threshold") {
            options.threshold = parseNumber(nextArg(argc, argv, i, arg), 0, 255, arg);
        }
        else if (arg == "--threads") {
            options.threadCount = static_cast<unsigned int>(
                parseNumber(nextArg(argc, argv, i, arg), 1, 256, arg));
        }
        else if (arg == "--log") {
            options.logPath = nextArg(argc, argv, i, arg);
        }
        else {
            throw ArgumentError("Unknown option: " + arg);
        }
    }

    if (options.inputPath.empty()) throw ArgumentError("--input is required");
    if (options.outputPath.empty()) throw ArgumentError("--output is required");
    if (options.filterName.empty()) throw ArgumentError("--filter is required");

    // 제공 코드의 threshold:128 표기도 단일 필터 인자로 지원한다.
    const std::string prefix = "threshold:";
    if (options.filterName.rfind(prefix, 0) == 0) {
        if (seen.count("--threshold") != 0) {
            throw ArgumentError("Specify threshold only once");
        }
        options.threshold = parseNumber(options.filterName.substr(prefix.size()), 0, 255, "threshold");
        options.filterName = "threshold";
    }
    if (seen.count("--threshold") != 0 && options.filterName != "threshold") {
        throw ArgumentError("--threshold requires --filter threshold");
    }
    return options;
}

void CommandLineParser::printUsage(const std::string& exeName) {
    std::cout
        << "Usage:\n  " << exeName << " --input <path> --output <path> --filter <name> [options]\n\n"
        << "Options:\n"
        << "  -i, --input   <path>   Input BMP (24-bit, uncompressed)\n"
        << "  -o, --output  <path>   Output BMP\n"
        << "  -f, --filter  <name>   grayscale, threshold, or threshold:N\n"
        << "      --threshold <N>   Threshold: 0..255 (default: 128; threshold filter only)\n"
        << "      --threads   <N>   Workers: 1..256 (default: hardware concurrency)\n"
        << "      --log    <path>   Append log (default: ImageProcessor.log)\n"
        << "  -h, --help            Show this message\n\n"
        << "Examples:\n"
        << "  " << exeName << " -i input.bmp -o gray.bmp -f grayscale --threads 4\n"
        << "  " << exeName << " -i input.bmp -o binary.bmp -f threshold --threshold 128\n";
}

} // namespace ip
