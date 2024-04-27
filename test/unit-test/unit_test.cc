#include "UnitTest++/UnitTest++.h"
#include "fixed_types.h"
#include "ep_agent.h"
#include <fstream>
#include <assert.h>
#include <unistd.h>
#include <set>
#include <algorithm>

int get_intersection_size(std::set<uint64_t>& s1, std::set<uint64_t>& s2) {
    std::vector<uint64_t> temp(std::max(s1.size(), s2.size()));
    auto it = std::set_intersection(s1.begin(), s1.end(), s2.begin(), s2.end(), temp.begin());
    return std::distance(temp.begin(), it);
    // printf("Intersection: %lu\n", std::distance(temp.begin(), it));
}

SUITE(CuckooHashMapTest) {
    TEST(ReadLog) {
        CxTnLMemShim::EPAgent agent;
        char cwd[11451];
        assert(getcwd(cwd, sizeof(cwd)) != NULL);
        std::ifstream ifs(std::string(cwd) + "/../cxl3_dram_trace_c0.log");
        CHECK(ifs);

        // Replay the memory access trace
        std::string line;
        while (std::getline(ifs, line)) {
            std::istringstream iss(line);
            uint64_t type, address, size;
            if (!(iss >> type >> address >> size)) {
                std::cerr << "Failed to parse the line: " << line << '\n';
                continue;
            }
            address &= ~0x3F;
            // Write Sync
            if (type == 2) {
                agent.AppendWorkQueueElement({address, size, 0});
                agent.dequeueWorkQueue();
            } else {
                IntPtr dummy = 0;
                agent.Translate(address, 0, type, dummy);
            }
        }
        CHECK(1);
    }

    TEST(CheckLog) {
        char cwd[11451];
        assert(getcwd(cwd, sizeof(cwd)) != NULL);
        std::ifstream ifs(std::string(cwd) + "/../cxl3_dram_trace_c0.log");
        CHECK(ifs);

        std::set<uint64_t> readset, writeset, flushset;
        // Replay the memory access trace
        std::string line;
        while (std::getline(ifs, line)) {
            std::istringstream iss(line);
            uint64_t type, address, size;
            if (!(iss >> type >> address >> size)) {
                std::cerr << "Failed to parse the line: " << line << '\n';
                continue;
            }
            uint64_t address_base = address & ~0x3f;
            for (uint64_t addr = address_base; addr < address + size; addr += (0x3f + 1)) {
                switch (type) {
                    case 0:
                        readset.insert(addr);
                        break;
                    case 1: 
                        writeset.insert(addr);
                        break;
                    case 2:
                        flushset.insert(addr);
                        break;
                }
            }
        }
        printf("Read-Write Intersection Size: %d\n", get_intersection_size(readset, writeset));
        printf("Read-Flush Intersection Size: %d\n", get_intersection_size(readset, flushset));
        printf("Write-Flush Intersection Size: %d\n", get_intersection_size(writeset, flushset));

        std::vector<uint64_t> temp(std::max(readset.size(), writeset.size()));
        auto it = std::set_intersection(readset.begin(), readset.end(), writeset.begin(), writeset.end(), temp.begin());
        temp.resize(it - temp.begin()); // Resize temp to the size of the intersection

        std::vector<uint64_t> intersection(std::max(temp.size(), flushset.size()));
        auto itt = std::set_intersection(temp.begin(), temp.end(), flushset.begin(), flushset.end(), intersection.begin());
        intersection.resize(itt - intersection.begin()); // Resize intersection to the size of the final intersection
        printf("Read-Write-Flush Intersection Size: %lu\n", intersection.size());

        printf("Unique Reads, Writes, Flushes: %lu, %lu, %lu\n", readset.size(), writeset.size(), flushset.size());
        // std::set_intersection(temp.begin(), temp.end(), flushset.begin(), flushset.end(), std::back_inserter(intersection));
    }
}


int main(int, const char *[])
{
   return UnitTest::RunAllTests();
}
