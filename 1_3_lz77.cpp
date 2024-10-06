#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <vector>
#include <fstream>

#include "common.hpp"

struct out
{
    /* {start_position, len, byte} */
    uint8_t bytes[3];
};

void largest_repeating_sequence(comp::Buffer &search, comp::Buffer &lookahead, struct out &result)
{
    /*     for (int i = 0; i < search.size; i++)
        {
            std::cout << search.at(i) << " ";
        }
        std::cout << "| ";

        for (int i = 0; i < lookahead.size; i++)
        {
            std::cout << lookahead.at(i) << " ";
        }
        std::cout << std::endl; */

    /* First try to find the largest substring from the lookahead comp::Buffer.
     * subsize - length of the lookahead substring to find
     */
    for (uint8_t subsize = lookahead.size - 1; subsize > 0; subsize--)
    {
        /* Start looking for the substring from the lowest position of the search comp::Buffer. */
        for (int i = search.size - 1; i >= 0; i--)
        {
            bool matching = true;
            int j;

            /* Search starting from the search comp::Buffer... */
            for (j = i; matching && j < search.size && j < i + subsize; j++)
            {
                matching = (search.at(j) == lookahead.at(j - i));
            }

            /* ...and into the lookahead comp::Buffer if possible: */
            for (j = 0; matching && j < i + subsize - search.size; j++)
            {
                matching = (lookahead.at(j) == lookahead.at(j + (search.size - i)));
            }

            /* Largest repeating sequence, possibly spanning into the lookahead comp::Buffer, has been found: */
            if (matching)
            {
                result = {.bytes = {static_cast<uint8_t>(i), subsize, lookahead.at(subsize)}};
                return;
            }
        }
    }
    /* Otherwise, a unique byte has been encountered, add it to the search comp::Buffer.
     *
     * Additionally, this case is triggered if all bytes in the lookahead comp::Buffer form
     * a substring somewhere in the search comp::Buffer. Example:
     *
     *       SEARCH[n+1]         LOOKAHEAD[3]
     *
     * a0, a1, a2, a3, ..., an | a1, a2, a3
     *
     * a1, a2 will be caught by the nested loops above, whereas a3 will be re-added to the search comp::Buffer as if was distinct,
     * regardless if it already exists in the search comp::Buffer. This does weaken the compression ratio, but only by a negligible amount.
     */
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
        /* Encode the file: */
        const int search_buffer_size = 226;
        const int lookahead_buffer_size = 24;
        const std::string out_filename = filename + extension;

        comp::Buffer search(search_buffer_size), lookahead(lookahead_buffer_size);

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

            for (int j = 0; j < len; j++)
            {
                auto pop = lookahead.pop();
                search.push(pop);

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
        /* Decode the file: */
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

                if (index_shift < 0)
                {
                    index_shift = 0;
                }
                for (int i = t.bytes[0]; i < t.bytes[0] + t.bytes[1]; i++)
                {
                    decoded.push_back(decoded[index_shift + i]);
                }
            }
            decoded.push_back(t.bytes[2]);
        }

        /* Dump contents: */
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