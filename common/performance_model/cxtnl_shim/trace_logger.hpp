#include <zlib.h>
#include "fixed_types.h"

struct DRAM_ACCESS {
    int access_type;
    IntPtr address_base;
    size_t size;
};

class Logger {
    std::vector<DRAM_ACCESS> buffer;
    gzFile file;
    size_t bufferSize;
    pthread_mutex_t* logger_latch;

public:
    Logger(const char* filename, size_t bufferSize)
        : bufferSize(bufferSize) {
        file = gzopen(filename, "wb");
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
            gzprintf(file, "%d %p %u\n", entry.access_type, entry.address_base, entry.size);
        }
        buffer.clear();
    }

    ~Logger() {
        flush();
        gzclose(file);
        delete logger_latch;
    }
};