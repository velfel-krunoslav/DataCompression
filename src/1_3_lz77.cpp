#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <vector>
#include <fstream>

class Buffer
{
private:
    Buffer();

public:
    const uint8_t maxbufsize;
    std::unique_ptr<uint8_t[]> buf;
    uint8_t size = 0;
    uint8_t begin = 0;

    ~Buffer() = default;
    Buffer(uint8_t sz) : maxbufsize(sz), buf(std::make_unique<uint8_t[]>(sz)) {}

    void push(uint8_t byte)
    {
        if (size == maxbufsize)
        {
            /* If full, make space by deleting the least recently added: */
            pop();
        }

        /* TODO: is the cast necessary to avoid integer overflow? */
        uint16_t idx = static_cast<uint16_t>(begin) + size;
        idx = idx % maxbufsize;

        buf.get()[idx] = byte;

        size++;
    }

    uint8_t pop()
    {
        if (size == 0)
        {
            /* TODO: find a better solution */
            return 0;
        }

        uint8_t val = buf.get()[begin];

        if (begin == maxbufsize - 1)
        {
            begin = 0;
        }
        else
        {
            begin++;
        }
        size--;
        return val;
    }

    uint8_t at(uint8_t idx)
    {
        return buf.get()[(begin + idx) % maxbufsize];
    }
};

struct out
{
    /* {start_position, len, byte} */
    uint8_t bytes[3];
};

void largest_repeating_sequence(Buffer &search, Buffer &lookahead, struct out &result)
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

    /* First try to find the largest substring from the lookahead buffer.
     * subsize - length of the lookahead substring to find
     */
    for (uint8_t subsize = lookahead.size - 1; subsize > 0; subsize--)
    {
        /* Start looking for the substring from the lowest position of the search buffer. */
        for (int i = search.size - 1; i >= 0; i--)
        {
            bool matching = true;
            int j;

            /* Search starting from the search buffer... */
            for (j = i; matching && j < search.size && j < i + subsize; j++)
            {
                matching = matching && (search.at(j) == lookahead.at(j - i));
            }

            /* ...and into the lookahead buffer if possible: */
            for (j = 0; matching && j < i + subsize - search.size; j++)
            {
                matching = matching && (lookahead.at(j) == lookahead.at(j + (search.size - i)));
            }

            /* Largest repeating sequence, possibly spanning into the lookahead buffer, has been found: */
            if (matching)
            {
                result = {.bytes = {static_cast<uint8_t>(i), subsize, lookahead.at(subsize)}};
                return;
            }
        }
    }
    /* Otherwise, a unique byte has been encountered, add it to the search buffer.
     *
     * Additionally, this case is triggered if all bytes in the lookahead buffer form
     * a substring somewhere in the search buffer. Example:
     *
     *       SEARCH[n+1]         LOOKAHEAD[3]
     *
     * a0, a1, a2, a3, ..., an | a1, a2, a3
     *
     * a1, a2 will be caught by the nested loops above, whereas a3 will be re-added to the search buffer as if was distinct,
     * regardless if it already exists in the search buffer. This does weaken the compression ratio, but only by a negligible amount.
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

    static const int search_buffer_size = 64;
    static const int lookahead_buffer_size = 192;

    static_assert(search_buffer_size + lookahead_buffer_size == 256, "Adjust buffer sizes");
    
    if (mode == "-e")
    {
        /* Encode the file: */

    }
    else if (mode == "-d")
    {
        /* Decode the file: */

    }
    else
    {
        std::cout << "Usage: {-e|-d} <filename>" << std::endl;
        return EXIT_FAILURE;
    }


    Buffer search(search_buffer_size), lookahead(lookahead_buffer_size);

    uint16_t i;
    for (i = 0; i < lookahead_buffer_size; i++)
    {
        lookahead.push(val[i % 10]);
    }

    std::vector<struct out> encoded;
    std::vector<uint8_t> decoded;

    while (lookahead.size)
    {
        struct out result;

        largest_repeating_sequence(search, lookahead, result);

        encoded.push_back(result);

        printf("%2d,%2d,%2c\n", result.bytes[0], result.bytes[1], result.bytes[2]);

        uint8_t len = result.bytes[1];

        len++;

        for (int j = 0; j < len; j++)
        {
            auto pop = lookahead.pop();
            if (i < (sizeof(val) / sizeof(val[0])))
            {
                lookahead.push(val[i]);
                i++;
            }
            search.push(pop);
        }
    }

    for (auto &t : encoded)
    {
        if (t.bytes[1])
        {
            int index_shift = decoded.size() - 4;

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

    for (auto t : decoded)
    {
        std::cout << t << " ";
    }

    std::cout << std::endl;

    return EXIT_SUCCESS;
}