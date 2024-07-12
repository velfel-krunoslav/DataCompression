#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <vector>
#include <fstream>

#include "common.hpp"

/* Prefix tree (dictionary): */
struct node
{
    static const size_t bsize = 256;

    uint16_t i;
    struct node *bytes[bsize];

    node(uint16_t idx) : i(idx)
    {
        for (int i = 0; i < bsize; i++)
        {
            bytes[i] = nullptr;
        }
    }

    ~node()
    {
        for (int i = 0; i < bsize; i++)
        {
            if (bytes[i])
            {
                delete bytes[i];
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
    const size_t maxsize = 0xFFFFFF;
    size_t count;

    Dictionary() : root(new struct node(0)), count(0)
    {
        /* Populate the dictionary with bytes 0, ..., 255 ahead of time: */
        for (uint16_t idx = 0; idx < node::bsize; idx++)
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

        if (n->bytes[byte])
        {
            return false;
        }
        n->bytes[byte] = new struct node(count);
        count++;
        return true;
    }
};

/* Helper function to write bits to an output stream when the bit count not divisible by 8: */
inline void write_stream(std::ofstream &out, uint8_t &buf, size_t &buf_size, uint32_t byte, size_t valid_bits)
{
    byte <<= (32 - valid_bits);

    for (uint32_t t = 0x1 << 31; valid_bits; t >>= 1, valid_bits--)
    {
        if (buf_size >= 8)
        {
            //printf("%02X ", buf);
            out.write(reinterpret_cast<char *>(&buf), sizeof(buf));
            buf = 0;
            buf_size = 0;
        }
        buf = buf | (((byte & t) ? 0x80 : 0) >> buf_size);
        buf_size++;
    }
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
    const std::string extension(".lzw");

    if (mode == "-e")
    {
        /* Encode the file: */
        const std::string out_filename = filename + extension;

        Dictionary dict;

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

        size_t size = dict.maxsize;
        uint8_t valid_bits = 0;

        while (size)
        {
            valid_bits++;
            size >>= 1;
        }

        uint8_t byte;
        uint8_t buf = 0;
        size_t buf_size = 0;

        struct node *t = dict.root;
        while (in.read(reinterpret_cast<char *>(&byte), sizeof(byte)))
        {
            auto is_new = dict.try_insert(t, byte);

            if (is_new)
            {
                auto idx = t->bytes[byte]->i;
                // printf("%8d | %08X\n", idx, byte);

                write_stream(out, buf, buf_size, idx, valid_bits);
                t = dict.root;
            }
            else
            {
                if (t->bytes[byte] == nullptr)
                {
                    /* Out of space, keep adding bytes anyway: */
                    if (t != dict.root)
                    {
                        auto idx = t->i;
                        write_stream(out, buf, buf_size, idx, valid_bits);
                    }
                    auto idx = byte;
                    write_stream(out, buf, buf_size, idx, valid_bits);

                    t = dict.root;
                }
                else
                {
                    t = t->bytes[byte];
                }
            }
        }

        if (t != dict.root)
        {
            /* TODO: is this necessary? Test and prove it. */
            /* Byte stream ends on a byte sequence that is in the dictionary */
            /* Leftover: */
            auto idx = t->i;
            write_stream(out, buf, buf_size, idx, valid_bits);
        }

        std::cout << std::endl << "Valid bits:" << static_cast<int>(valid_bits) << std::endl;
        std::cout << std::endl << "Dict count:" << dict.count << std::endl;

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

        /*         uint8_t search_buffer_size;
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
         */
        /* Dump contents: */
        /*         out.write(reinterpret_cast<const char *>(decoded.data()), decoded.size());
         */
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