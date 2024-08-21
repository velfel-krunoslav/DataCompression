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

#include "common.hpp"

/* TODO: check inline
 * Helper function to write bits to an output stream when the bit count not divisible by 8: */
inline void write_stream(std::vector<char> &out, uint8_t &buf, size_t &buf_size, uint32_t byte, size_t valid_bits)
{
    byte <<= (32 - valid_bits);

    for (uint32_t t = 0x1 << 31; valid_bits; t >>= 1, valid_bits--)
    {
        if (buf_size >= 8)
        {
            // printf("%02X ", buf);
            char q = buf;
            out.push_back(q);
            buf = 0;
            buf_size = 0;
        }
        buf = buf | (((byte & t) ? 0x80 : 0) >> buf_size);
        buf_size++;
    }
}

/* Prefix tree (dictionary): */
struct node
{
    static const size_t maxbytes = 256;
    size_t count = 0;
    std::unordered_map<uint8_t, std::pair<struct node *, size_t>> bytes;

    struct node *parent = nullptr;

    bool push(uint8_t byte, size_t i)
    {
        if (count == maxbytes)
        {
            return false;
        }

        if (bytes.find(byte) == bytes.end())
        {
            bytes[byte] = std::make_pair(nullptr, i);
            count++;

            return true;
        }

        return false;
    }

    ~node()
    {
        for (auto &[byte, pair] : bytes)
        {
            if (pair.first)
            {
                delete pair.first;
            }
        }
    }

    void flush(std::vector<char> &out, uint8_t &buf, size_t &buf_size, size_t size_bits)
    {
        size_t mode = 0x0;

        auto nullcount = maxbytes - bytes.size();

        /* TODO: there may be a beter cutoff limit than (bsize / 2), which may improve the compression ratio even further: */
        if (nullcount > (maxbytes / 2))
        {
            mode = 0x1;
        }

        write_stream(out, buf, buf_size, mode, 1);

        for (size_t idx = 0; idx < maxbytes; idx++)
        {
            if (bytes.find(idx) != bytes.end())
            {
                if (mode == 0x1)
                {
                    write_stream(out, buf, buf_size, idx, size_bits);
                }
                write_stream(out, buf, buf_size, bytes[idx].second, size_bits);
            }
            else if (mode == 0x0)
            {
                write_stream(out, buf, buf_size, 0x0, size_bits);
            }
        }

        if (mode == 0x1)
        {
            write_stream(out, buf, buf_size, 0x0, 1);
        }

        for (auto [byte, pair] : bytes)
        {
            if (pair.first)
            {
                pair.first->flush(out, buf, buf_size, size_bits);
            }
        }
    }
};

class Dictionary
{
public:
    struct node *root;
    /* Dictionary size:
     * https://jrsoftware.org/ishelp/index.php?topic=setup_lzmadictionarysize
     */
    /* 0xFFFFF max recommended, consumes 2GB RAM (stale info, test again) */
    const size_t maxsize = 0xFFFFF;
    size_t count;

    Dictionary()
    {
        root = new struct node();
        count = 0;

        /* Populate the dictionary with bytes 0, ..., 255 ahead of time: */
        for (size_t idx = 0; idx < node::maxbytes; idx++)
        {
            try_insert(root, idx);
        }
    }

    ~Dictionary()
    {
        delete root;
    }

    bool try_insert(struct node *n, uint8_t byte)
    {
        if (count == maxsize || n == nullptr)
        {

            return false;
        }

        if (n->bytes.find(byte) != n->bytes.end())
        {
            if (n->bytes[byte].first)
            {

                return false;
            }
        }

        count++;
        auto inserted = n->push(byte, count);

        if (!inserted)
        {
            count--;
        }

        return inserted;
    }
};

int main(int argc, char *argv[])
{
    Dictionary dict;

    auto t = dict.root;

    t->bytes[0].first = new struct node;
    t->bytes[0].first->parent = t;
    
    std::cout << dict.try_insert(t->bytes[0].first, 0x95) << std::endl;
    /*
    t = t->bytes[0x95].first;
    dict.try_insert(t, 0x96);

    dict.try_insert(t, 1);
    dict.try_insert(t, 2);
    dict.try_insert(t, 3); */

    dict.root->print();

    return EXIT_SUCCESS;
}