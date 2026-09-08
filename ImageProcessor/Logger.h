#pragma once

#include "CommandLineParser.h"

#include <fstream>
#include <string>

namespace ip {

struct RunLog {
    bool success = false;
    double elapsedMs = 0.0;
    double filterMs = 0.0;
    unsigned int actualThreads = 0;
    std::string message;
};

// 작업 스레드를 모두 join한 뒤 main에서 한 번 기록하므로 로그용 잠금은 필요 없다.
class Logger {
public:
    void open(const ProgramOptions& options);
    bool isOpen() const noexcept { return m_file.is_open(); }
    void write(const ProgramOptions& options, const RunLog& record);

private:
    std::ofstream m_file;
};

} // namespace ip
