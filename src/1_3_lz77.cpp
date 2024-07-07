#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <vector>

#define SEARCH_BUF_SIZE 64
#define LOOKAHEAD_BUF_SIZE 192

static_assert(SEARCH_BUF_SIZE + LOOKAHEAD_BUF_SIZE == 256, "Adjust buffer sizes");

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
    for (int i = 0; i < search.size; i++)
    {
        std::cout << search.at(i) << " ";
    }
    std::cout << "| ";

    for (int i = 0; i < lookahead.size; i++)
    {
        std::cout << lookahead.at(i) << " ";
    }
    std::cout << std::endl;
    /* First try to find the largest substring from the lookahead buffer.
     * subsize - length of the lookahead substring to find
     */
    for (uint8_t subsize = lookahead.size - 1; subsize > 0; subsize--)
    {
        /* Start looking for the substring from the lowest position of the search buffer. */
        for (int i = search.size - 1; i >= 0; i--)
        {
            bool matching = true;
            int j = i;

            /* Search starting from the search buffer... */
            for (; matching && j < search.size; j++)
            {
                matching = matching && (search.at(j) == lookahead.at(j - i));
            }

            /* ...and into the lookahead buffer if possible: */
            for (j = 0; matching && j < subsize - (search.size - i); j++)
            {
                matching = matching && (lookahead.at(j) == lookahead.at(j + (search.size - i)));
            }

            /* Largest repeating sequence, possibly spanning into the lookahead buffer, has been found: */
            if (matching)
            {
                result = {.bytes = {static_cast<uint8_t>(i), subsize, lookahead.at(subsize)}};
                printf("<%d, %d, %c>\n\n", result.bytes[0], result.bytes[1], result.bytes[2]);
                return;
            }
        }
    }
    /* Otherwise, a unique byte has been encountered, add it to the search buffer.
     *
     * Additionally, this case is triggered if all the remaining bytes in the lookahead buffer form
     * a substring somewhere in the search buffer. Example:
     * 
     *       SEARCH                LOOKAHEAD
     * 
     * a0, a1, a2, a3, ..., an | a1, a2, a3, eof
     * 
     * a1, a2 will be caught by the nested loops above, whereas a3 will be re-added as if was a distinct search buffer byte.
     * This does weaken the compression ratio, but by a negligible amount.
     */
    result = {.bytes = {0, 0, lookahead.at(0)}};
    printf("<%d, %d, %c>\n\n", result.bytes[0], result.bytes[1], result.bytes[2]);
}

int main(void)
{
    Buffer search(4), lookahead(4);

    uint8_t val[] = {'A', 'B', 'A', 'B', 'C', 'B', 'C', 'B', 'A', 'A', 'B', 'C', 'A', 'B', 'B'};

    uint16_t i;
    for (i = 0; i < 4; i++)
    {
        lookahead.push(val[i % 10]);
    }

    while (lookahead.size)
    {
        struct out result;

        largest_repeating_sequence(search, lookahead, result);

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

    return EXIT_SUCCESS;
}