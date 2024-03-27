#include "UnitTest++/UnitTest++.h"
#include "fixed_types.h"
#include "ep_agent.h"
#include <fstream>
#include <assert.h>
#include <unistd.h>


SUITE(CuckooHashMapTest) {
    TEST(ReadLog) {
        CxTnLMemShim::EPAgent agent;
        char cwd[11451];
        assert(getcwd(cwd, sizeof(cwd)) != NULL);
        std::ifstream ifs(std::string(cwd) + "/../cxl3_dram_trace_c0.log");
        if (!ifs) {
            std::cerr << "Failed to open the file.\n";
            CHECK(0);    
        }

        std::string line;
        while (std::getline(ifs, line)) {
            std::istringstream iss(line);
            uint64_t type, address;
            if (!(iss >> type >> address)) {
            std::cerr << "Failed to parse the line: " << line << '\n';
                continue;
            }
            IntPtr dummy = 0;
            agent.Translate(address, 0, type, dummy);
        }
        CHECK(1);
    }
}


int main(int, const char *[])
{
   return UnitTest::RunAllTests();
}
