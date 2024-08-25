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

size_t get_chunk_from_buffer(std::vector<uint8_t> &buffer, size_t &byte_idx, uint8_t &bit_idx, uint8_t bit_count)
{
    if (bit_count > 64)
    {
        std::cerr << "Bit limit exceeded " << __LINE__ << std::endl;
        exit(EXIT_FAILURE);
    }

    size_t value = 0;

    for (auto t = 0; t < bit_count; t++, bit_idx++)
    {
        size_t mask = 0x1UL << (bit_count - 1 - t);
        if (bit_idx >= 8)
        {
            bit_idx = 0;
            byte_idx++;
        }
        value |= ((buffer[byte_idx] & (0x80 >> bit_idx)) ? mask : 0);
    }
    printf("\n");

    return value;
}

int main(int argc, char *argv[])
{
    /*     std::ofstream out("0xFF_0xF0.dat", std::ios::binary);
     */
    uint8_t buf[] = {
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

    std::vector<uint8_t> vec;

    for (size_t i = 0; i < (sizeof(buf) / sizeof(buf[0])); i++)
    {
        vec.push_back(buf[i]);
    }

    uint8_t bit_idx = 0;
    size_t byte_idx = 0;

    printf("%X\n", get_chunk_from_buffer(vec, byte_idx, bit_idx, 34));

    /*     out.write(reinterpret_cast<char*>(buf), sizeof(buf));

        out.close();
     */
    return EXIT_SUCCESS;
}