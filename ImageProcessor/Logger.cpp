#include "Logger.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <iomanip>
#include <stdexcept>
#include <system_error>

namespace ip {
namespace {

bool samePath(const std::string& first, const std::string& second) {
    if (second.empty()) return false;
    std::error_code error;
    if (std::filesystem::equivalent(first, second, error)) return true;

    auto left = std::filesystem::weakly_canonical(first).native();
    auto right = std::filesystem::weakly_canonical(second).native();
#ifdef _WIN32
    // 아직 생성되지 않은 Windows 경로도 대소문자만 다르면 동일하게 취급한다.
    const auto lower = [](wchar_t value) { return static_cast<wchar_t>(std::towlower(value)); };
    std::transform(left.begin(), left.end(), left.begin(), lower);
    std::transform(right.begin(), right.end(), right.begin(), lower);
#endif
    return left == right;
}

std::string singleLine(std::string text) {
    std::replace(text.begin(), text.end(), '\n', ' ');
    std::replace(text.begin(), text.end(), '\r', ' ');
    return text;
}

} // namespace

void Logger::open(const ProgramOptions& options) {
    if (samePath(options.logPath, options.inputPath) || samePath(options.logPath, options.outputPath)) {
        throw std::runtime_error("[Log] Log path must differ from input/output paths");
    }
    m_file.open(options.logPath, std::ios::app);
    if (!m_file) {
        throw std::runtime_error("[Log] Cannot open log file: " + options.logPath);
    }
}

void Logger::write(const ProgramOptions& options, const RunLog& record) {
    m_file << std::fixed << std::setprecision(3)
           << "status=" << (record.success ? "success" : "failure")
           << " input=" << std::quoted(singleLine(options.inputPath))
           << " output=" << std::quoted(singleLine(options.outputPath))
           << " filter=" << std::quoted(singleLine(options.filterName))
           << " threshold=";
    if (options.filterName == "threshold") m_file << options.threshold;
    else m_file << "n/a";
    m_file << " requested_threads=";
    if (options.threadCount == 0) m_file << "auto";
    else m_file << options.threadCount;
    m_file << " actual_threads=" << record.actualThreads
           << " filter_ms=" << record.filterMs
           << " elapsed_ms=" << record.elapsedMs
           << " message=" << std::quoted(singleLine(record.message)) << '\n';
    m_file.flush();
    if (!m_file) throw std::runtime_error("[Log] Failed to write log file: " + options.logPath);
}

} // namespace ip
