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
    const uint8_t maxbufsize;
    Buffer();

public:
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

    bool pop()
    {
        if (size == 0)
        {
            return false;
        }

        if (begin == maxbufsize - 1)
        {
            begin = 0;
        }
        else
        {
            begin++;
        }
        size--;
        return true;
    }

    void print()
    {
        uint16_t idx;
        for (uint8_t i; i < size; i++)
        {
            idx = begin + i;
            idx = idx % maxbufsize;

            std::cout << static_cast<int>(buf.get()[idx]) << " ";
        }
        std::cout << std::endl;
    }
};

struct out
{
    /* {start_position, len, byte} */
    uint8_t bytes[3];
};

void largest_repeating_sequence(Buffer &search, Buffer &lookahead, struct out &result)
{
    /* First try to find the largest substring from the lookahead buffer.
     * i - length of the lookahead substring to find
     */
    for (int i = lookahead.size; i > 0; i--)
    {
        /* Start looking for the substring from the lowest position of the search buffer. */
        for (int j = 0; j < search.size; j++)
        {
            bool matching = true;
            int k = 0, l = 0;

            /* Search starting from the search buffer... */
            for(; matching && k <= j; j++) {
                matching = matching && (search.buf.get()[k] == lookahead.buf.get()[k]);
            }

            l = k;

            /* ...and into the lookahead buffer if possible: */
            for(; matching && k < i; k++) {
                matching = matching && (search.buf.get()[k - l] == lookahead.buf.get()[k]);
            }

            if(matching)
        }
    }
}

int main(void)
{
    Buffer search(SEARCH_BUF_SIZE); //, lookahead(LOOKAHEAD_BUF_SIZE)

    uint8_t val[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    for (uint16_t i = 0; i < 300; i++)
    {
        search.push(val[i % 10]);
        search.print();
    }

    return EXIT_SUCCESS;
}