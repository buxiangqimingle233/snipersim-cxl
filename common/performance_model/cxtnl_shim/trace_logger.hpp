#pragma once

#include <fstream>
#include <vector>
#include <utility>
#include "fixed_types.h"

namespace CxTnLMemShim {
class Logger {
    std::vector<std::pair<int, IntPtr>> buffer;
    std::ofstream file;
    size_t bufferSize;

public:
    Logger(const char* filename, size_t bufferSize)
        : file(filename), bufferSize(bufferSize) {
        if (!file) {
            throw std::runtime_error("Failed to open log file");
        }
    }

    void log(IntPtr pa, int access_type) {
        buffer.push_back(std::make_pair(access_type, pa));
        if (buffer.size() >= bufferSize) {
            flush();
        }
    }

    void flush() {
        for (const auto& entry : buffer) {
            file << entry.first << " " << entry.second << '\n';
        }
        buffer.clear();
    }

    ~Logger() {
        flush();
    }
};

}