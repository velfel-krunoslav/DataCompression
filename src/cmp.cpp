/*  CHECK .XLSX and examine why is the dictionary not getting filled to its maximum.
    Could count++ be the culprit?

    Also, test variable dictionary word size.
 */

#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <vector>
#include <fstream>
#include <unordered_map>
#include <map>
#include <fstream>

#include "common.hpp"

int main(int argc, char *argv[])
{
    std::ofstream out("0xFF_0xF0.dat", std::ios::binary);

    unsigned char buf[] = {
        0xFF,
        0xFE,
        0xFD,
        0xFC,
        0xFB,
        0xFA,
        0xF9,
        0xF8,
        0xF7,
        0xF6,
        0xF5,
        0xF4,
        0xF3,
        0xF2,
        0xF1,
        0xF0,
    };

    out.write(reinterpret_cast<char*>(buf), sizeof(buf));

    out.close();
    return EXIT_SUCCESS;
}