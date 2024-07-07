#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <vector>
#include <fstream>
#include <string>

#include "common.hpp"

class Buffer
{
public:
    const uint8_t maxbufsize;
    std::unique_ptr<uint8_t[]> buf;
    uint8_t size = 0;
    uint8_t begin = 0;

    Buffer(uint8_t sz) : maxbufsize(sz), buf(std::make_unique<uint8_t[]>(sz)) {}

    inline void push(uint8_t byte)
    {
        if (size == maxbufsize)
        {
            pop();
        }

        uint8_t idx = (begin + size) % maxbufsize;
        buf[idx] = byte;
        size++;
    }

    inline uint8_t pop()
    {
        if (size == 0)
        {
            return 0; // Consider handling this case differently
        }

        uint8_t val = buf[begin];
        begin = (begin + 1) % maxbufsize;
        size--;
        return val;
    }

    inline uint8_t at(uint8_t idx) const
    {
        return buf[(begin + idx) % maxbufsize];
    }
};

struct out
{
    uint8_t bytes[3];
};

void largest_repeating_sequence(Buffer &search, Buffer &lookahead, struct out &result)
{
    for (uint8_t subsize = lookahead.size - 1; subsize > 0; subsize--)
    {
        for (int i = search.size - 1; i >= 0; i--)
        {
            bool matching = true;
            for (int j = 0; matching && j < subsize; j++)
            {
                uint8_t searchIdx = (i + j) % search.maxbufsize;
                uint8_t lookaheadIdx = j % lookahead.maxbufsize;
                matching = (search.at(searchIdx) == lookahead.at(lookaheadIdx));
            }

            if (matching)
            {
                result = {.bytes = {static_cast<uint8_t>(i), subsize, lookahead.at(subsize)}};
                return;
            }
        }
    }
    result = {.bytes = {0, 0, lookahead.at(0)}};
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        std::cout << "Usage: {-e|-d} <filename>" << std::endl;
        return EXIT_FAILURE;
    }

    const std::string mode(argv[1]);
    const std::string filename(argv[2]);
    const std::string extension(".lz77");

    if (mode == "-e")
    {
        const int search_buffer_size = 64;
        const int lookahead_buffer_size = 64;
        const std::string out_filename = filename + extension;

        static_assert(search_buffer_size + lookahead_buffer_size <= 256, "Adjust buffer sizes");

        Buffer search(search_buffer_size), lookahead(lookahead_buffer_size);

        std::ifstream in(filename, std::ios::binary);
        std::ofstream out(out_filename, std::ios::binary);

        if (!in)
        {
            std::cerr << "Error opening file " << filename << std::endl;
            return EXIT_FAILURE;
        }
        if (!out)
        {
            std::cerr << "Error opening file " << out_filename << std::endl;
            return EXIT_FAILURE;
        }

        uint8_t byte;
        while (in.read(reinterpret_cast<char *>(&byte), sizeof(byte)) && lookahead.size < lookahead_buffer_size)
        {
            lookahead.push(byte);
        }

        out.write(reinterpret_cast<const char *>(&search_buffer_size), sizeof(search_buffer_size));
        out.write(reinterpret_cast<const char *>(&lookahead_buffer_size), sizeof(lookahead_buffer_size));

        while (lookahead.size)
        {
            struct out result;
            largest_repeating_sequence(search, lookahead, result);

            for (uint8_t b : result.bytes)
            {
                out.write(reinterpret_cast<char *>(&b), sizeof(b));
            }

            uint8_t len = result.bytes[1] + 1;

            for (uint8_t j = 0; j < len; j++)
            {
                uint8_t popped = lookahead.pop();
                search.push(popped);

                if (in.read(reinterpret_cast<char *>(&byte), sizeof(byte)))
                {
                    lookahead.push(byte);
                }
            }
        }

        in.close();
        out.close();
    }
    else if (mode == "-d")
    {
        const std::string out_filename = comp::common::trim_string_ext(filename);

        std::ifstream in(filename, std::ios::binary);
        std::ofstream out(out_filename, std::ios::binary);

        if (!in)
        {
            std::cerr << "Error opening file " << filename << std::endl;
            return EXIT_FAILURE;
        }
        if (!out)
        {
            std::cerr << "Error opening file " << out_filename << std::endl;
            return EXIT_FAILURE;
        }

        uint8_t search_buffer_size;
        uint8_t lookahead_buffer_size;

        in.read(reinterpret_cast<char *>(&search_buffer_size), sizeof(search_buffer_size));
        in.read(reinterpret_cast<char *>(&lookahead_buffer_size), sizeof(lookahead_buffer_size));

        struct out t;
        std::vector<uint8_t> decoded;

        while (in.read(reinterpret_cast<char *>(&t.bytes), sizeof(t.bytes)))
        {
            if (t.bytes[1])
            {
                int index_shift = decoded.size() - search_buffer_size;
                index_shift = std::max(index_shift, 0);

                for (int i = 0; i < t.bytes[1]; i++)
                {
                    decoded.push_back(decoded[index_shift + t.bytes[0] + i]);
                }
            }
            decoded.push_back(t.bytes[2]);
        }

        out.write(reinterpret_cast<const char *>(decoded.data()), decoded.size());

        in.close();
        out.close();
    }
    else
    {
        std::cout << "Usage: {-e|-d} <filename>" << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
