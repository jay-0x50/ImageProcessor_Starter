#pragma once

/**
 * @file CommandLineParser.h
 * @brief 커맨드라인 인자 파싱.
 *
 * 기본 옵션과 이진화 임계값, 스레드 수, 로그 경로를 파싱한다.
 */

#include <string>

namespace ip {

/// 파싱 결과를 담는 단순 구조체.
struct ProgramOptions {
    std::string inputPath;   ///< --input  / -i
    std::string outputPath;  ///< --output / -o
    std::string filterName;  ///< --filter / -f
    int threshold = 128;    ///< --threshold (0~255)
    unsigned int threadCount = 0; ///< --threads (0: 자동, CLI 명시 값은 1~256)
    std::string logPath = "ImageProcessor.log"; ///< --log
    bool showHelp = false;
};

class CommandLineParser {
public:
    CommandLineParser() = delete;  // 인스턴스화 금지 (정적 메서드만 제공)

    /**
     * @brief argv 를 파싱하여 ProgramOptions 를 반환한다.
     * @throws ArgumentError 필수 인자 누락, 알 수 없는 옵션, 형식 오류 등.
     */
    static ProgramOptions parse(int argc, char* argv[]);

    /// 사용법을 표준 출력에 출력한다.
    static void printUsage(const std::string& exeName);
};

} // namespace ip
