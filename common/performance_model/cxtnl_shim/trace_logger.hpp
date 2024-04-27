#pragma once

#include <fstream>
#include <vector>
#include <utility>
#include "fixed_types.h"

namespace CxTnLMemShim {
typedef struct {
    int access_type;
    IntPtr address_base;
    unsigned int size;
} DRAM_ACCESS;

class Logger {
    std::vector<DRAM_ACCESS> buffer;
    std::ofstream file;
    size_t bufferSize;
    pthread_mutex_t* logger_latch;

public:
    Logger(const char* filename, size_t bufferSize)
        : file(filename), bufferSize(bufferSize) {
        if (!file) {
            throw std::runtime_error("Failed to open log file");
        }
        logger_latch = new pthread_mutex_t;
        pthread_mutex_init(logger_latch, NULL);
    }

    void log(DRAM_ACCESS trace) {
        pthread_mutex_lock(logger_latch);
        buffer.push_back(trace);
        if (buffer.size() >= bufferSize) {
            flush();
        }
        pthread_mutex_unlock(logger_latch);
    }

    void flush() {
        for (const auto& entry : buffer) {
            file << entry.access_type << " " << entry.address_base << " " << entry.size << '\n';
        }
        buffer.clear();
    }

    ~Logger() {
        flush();
        delete logger_latch;
    }
};

}