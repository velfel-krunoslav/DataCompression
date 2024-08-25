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

#include "common.hpp"

/* Helper function to write bits to an output stream when the bit count not divisible by 8: */
inline void _write_stream(std::vector<char> &out, uint8_t &buf, size_t &buf_size, uint32_t byte, size_t word_width)
{
    /*     std::cout << static_cast<int>(byte) << std::endl;
     */
    byte <<= (32 - word_width);

    for (uint32_t t = 0x1 << 31; word_width; t >>= 1, word_width--)
    {
        buf = buf | (((byte & t) ? 0x80 : 0) >> buf_size);
        buf_size++;
        if (buf_size >= 8)
        {
            // printf("%02X ", buf);
            char q = buf;
            out.push_back(q);
            buf = 0;
            buf_size = 0;
        }
    }

    if(buf_size != 0) {
        printf("||%2X||", buf);
    }
}


#define write_stream(a, b, c, d, e) do{printf("Writing: %X %d\n", d, e);_write_stream(a, b, c, d, e);for (auto t : a){printf("|%02X", t);}printf("\n\n");}while(0)

/* Prefix tree (dictionary): */
struct node
{
    static const size_t maxbytes = 256;

    size_t count;
    std::unordered_map<uint8_t, std::pair<struct node *, size_t>> bytes;

    node() : count(0) {}

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

    /* Set `this->bytes[byte]`flag to `i`: */
    bool insert(uint8_t byte, size_t i)
    {
        if (bytes.find(byte) == bytes.end())
        {
            count++;
            bytes[byte] = std::make_pair(nullptr, i);
            return true;
        }
        return false;
    }

    void get_tree_stats(std::vector<size_t> &depths, size_t depth = 0)
    {
        for (auto &[byte, pair] : bytes)
        {
            if (pair.first)
            {
                pair.first->get_tree_stats(depths, depth + 1);
            }
        }

        if (bytes.size() == 0)
        {
            depths.push_back(depth);
        }
    }

    /* Serialize the dictionary (i.e. the trie). Performed breadth-first.
     *
     * There are two distinct serialization methods:
     *     0. For every byte (that is, `bytes[idx]`) write only the `i` that the child points to:
     *                                                      `bytes[idx]->i`.
     *
     *        If a child (bytes[idx]) is nullptr, simply write 0x0.
     *
     *     1. Consider only the node's non-nullptr children. First write the the respective index of the child, followed by the `i` the child points to:
     *                                                   `idx | bytes[idx]->i`.
     *        End the serialization with 0x0.
     *
     * Method 0) offers better space utilization when the tree grows horizontally. If the tree at some point branches out, method 1) may be a better choice.
     * To differentiate between these two methods, before any data is serialized, first a single bit is written (with `0` denoting method 0, and `1` denoting method 1).
     *
     * `size_bits` is the least number of bits required to write the index `i` and depends on the total number of entries in the dictionary.
     * The `flush()` funciton is equipped to properly handle the scenario when this number is not divisible by 8, and compacts the bits, in series, without wasting any space.
     */
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
                    /* TODO: replace size_bits with width(type(idx)) */
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
            write_stream(out, buf, buf_size, 0x0, size_bits);
        }
    }

    void print(std::string indent = "->")
    {
        std::map m(bytes.begin(), bytes.end());

        for (auto [byte, pair] : m)
        {
            std::cout << indent;
            auto next = pair.first;
            auto idx = pair.second;
            printf("[%02X, %d]\n", byte, idx);
            if (next)
            {
                next->print(std::string("    ") + indent);
            }
        }
    }
};

size_t get_chunk_from_buffer(std::vector<uint8_t> &buffer, size_t &byte_idx, uint8_t &bit_idx, uint8_t bit_count)
{
    if (bit_count > 64)
    {
        std::cerr << "Bit limit exceeded " << __LINE__ << std::endl;
        exit(EXIT_FAILURE);
    }

    size_t value = 0;

/*     for (int i = 0; i < 8; i++)
    {
        printf("|%02X", buffer[byte_idx + i]);
    }
    printf("\n"); */

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
    
    printf("Reading: %02X %d\n", value, bit_count);

    return value;
}

class Dictionary
{
public:
    struct node *root;

    /* Iterators: */
    struct node *previous, *current;

    /* `previous->bytes[idx] == current` */
    uint8_t idx;

    /* Dictionary size:
     * https://jrsoftware.org/ishelp/index.php?topic=setup_lzmadictionarysize
     */
    /* 0xFFFFF max recommended, consumes 2GB RAM (stale info, test again) */
    const size_t maxsize = 0xFFFF;
    size_t count;
    uint8_t bit_idx;
    size_t byte_idx;

    Dictionary()
    {
        root = new struct node();
        reset();

        count = 0;

        try_status status;
        /* Populate the dictionary with bytes 0, ..., 255 ahead of time: */
        for (size_t byte = 0; byte < node::maxbytes; byte++)
        {
            root->insert(byte, 0);
        }

        /* Used by build_from_stream(): */
        bit_idx = 0;
        byte_idx = 0;
    }

    void reset()
    {
        idx = std::numeric_limits<unsigned int>::infinity();
        previous = current = root;
    }

    enum try_status
    {
        unset = 0,
        /* A new node has been inserted to accomodate the unique word being added to the dictionary: */
        new_node_or_flag = 1,

        /* Insertion was attempted but the dictionary is full: */
        no_space_left = 3,

        /* No insertions needed - just traverse the tree: */
        move_only = 4
    };

    void try_move_or_insert(uint8_t byte, try_status &status)
    {
        status = unset;

        if (count == maxsize)
        {
            status = no_space_left;
            reset();
            return;
        }

        if (current == nullptr)
        {
            previous->bytes[idx].first = current = new struct node();
        }

        if (current->bytes.find(byte) == current->bytes.end())
        {
            status = new_node_or_flag;
            current->bytes[byte].second = ++count;

            reset();
        }
        else
        {
            status = move_only;
        }

        previous = current;
        idx = byte;

        current = current->bytes[byte].first;
    }

    struct node **get_ptr(struct node *n, size_t parentidx, size_t byte)
    {
        for (auto &[b, child] : n->bytes)
        {
            if ((child.second == parentidx) && (b == byte))
            {
                return &(child.first);
            }
        }

        for (auto &[b, child] : n->bytes)
        {
            if (child.first)
            {
                return get_ptr(child.first, parentidx, byte);
            }
        }

        return nullptr;
    }

    void _flush(struct node *current, size_t parentidx, size_t currentidx, std::vector<char> &out, uint8_t &buf, size_t &buf_size, size_t size_bits)
    {
        write_stream(out, buf, buf_size, parentidx, size_bits);
        write_stream(out, buf, buf_size, currentidx, size_bits);

        current->flush(out, buf, buf_size, size_bits);

        for (auto &[byte, child] : current->bytes)
        {
            if (child.first)
            {
                _flush(child.first, child.second, byte, out, buf, buf_size, size_bits);
            }
        }
    }

    void flush(std::vector<char> &out, uint8_t &buf, size_t &buf_size, size_t size_bits)
    {
        for (auto &[byte, child] : root->bytes)
        {
            if (child.first)
            {
                reset();
                _flush(child.first, 0, byte, out, buf, buf_size, size_bits);
            }
        }
    }

    size_t build_from_stream(std::vector<uint8_t> &dict, uint8_t word_width)
    {
        size_t loaded_bytes = 0;

        /* TODO: word_width should be a class member */
        auto parentidx = get_chunk_from_buffer(dict, byte_idx, bit_idx, word_width);
        loaded_bytes += word_width;
        
        auto currentidx = get_chunk_from_buffer(dict, byte_idx, bit_idx, word_width);
        loaded_bytes += word_width;
        
        printf("Seek: parentidx = %d, currentidx = %d\n", parentidx, currentidx);

        auto child = get_ptr(root, parentidx, currentidx);
        if (child == nullptr)
        {
            printf("Child is null\n");
            exit(EXIT_FAILURE);
        }

        auto new_node = (*child) = new struct node();

        auto b = get_chunk_from_buffer(dict, byte_idx, bit_idx, 1);
        loaded_bytes += 1;
        
        if (b == 0)
        {
            printf("Mode 0)\n");
            // TODO, don't hardcode
            for (int i = 0; i < 256; i++)
            {
                auto points_to_idx = get_chunk_from_buffer(dict, byte_idx, bit_idx, word_width);
                loaded_bytes += word_width;
        
                if (points_to_idx)
                {
                    new_node->bytes[i].second = points_to_idx;
                }
            }
        }
        else
        {
            printf("Mode 1)\n");
            size_t vals[2];
            int iter = 0;
            while ((vals[iter] = get_chunk_from_buffer(dict, byte_idx, bit_idx, word_width)))
            {
                iter++;
                if (iter % 2 == 0)
                {
                    iter = 0;
                    auto byte = vals[0];
                    auto points_to_idx = vals[1];

                    new_node->bytes[byte].second = points_to_idx;
                }
                loaded_bytes += word_width;
            }
            loaded_bytes += word_width;
        }
        
        return loaded_bytes;
    }

    ~Dictionary()
    {
        delete root;
    }
};

std::ostream &operator<<(std::ostream &o, const Dictionary::try_status &s)
{
    switch (s)
    {
    case Dictionary::unset:
        o << "unset";
        break;
    case Dictionary::new_node_or_flag:
        o << "new_node_or_flag";
        break;
    case Dictionary::no_space_left:
        o << "no_space_left";
        break;
    default:
        o << "move_only";
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
        uint8_t word_width = 0;

        while (size)
        {
            word_width++;
            size >>= 1;
        }

        uint8_t byte;
        uint8_t buf = 0;
        size_t buf_size = 0;

        std::vector<char> outbuf;

        while (in.read(reinterpret_cast<char *>(&byte), sizeof(byte)))
        {
            Dictionary::try_status status;
            dict.try_move_or_insert(byte, status);

            // std::cout << status << std::endl;

            // std::cout << status << "[" << dict.depth << "]" << std::endl;

            if (status == Dictionary::try_status::new_node_or_flag)
            {
                /* This byte/sequence of bytes has never been encountered, and has just been added.
                 * Write its index in the compressed file: */
                write_stream(outbuf, buf, buf_size, dict.count, word_width);

                dict.reset();
            }
            else if (status == Dictionary::try_status::no_space_left)
            {

                /* Insertion has failed because the dictionary has no space left: */
                /* Firstly, handle the traversed substring from the dictionary:  */
                if (dict.previous != dict.current)
                {
                    write_stream(outbuf, buf, buf_size, dict.idx, word_width);
                }
                /* ...then separately output the byte in question without adding a new entry to the dictionary. */
                write_stream(outbuf, buf, buf_size, byte, word_width);
                dict.reset();
            }
        }

        if (dict.previous != dict.current)
        {
            write_stream(outbuf, buf, buf_size, dict.idx, word_width);
        }

        std::vector<char> dictout;
        uint8_t dictoutbuf = 0;
        size_t dictout_bufsize = 0;

        /* Write the dictionary contents: */
        dict.flush(dictout, dictoutbuf, dictout_bufsize, word_width);

        /* And the size of the dictionary itself: */
        auto dictout_size = dictout.size();
        printf("%d\n", dictout_size);
        for (int i = 0; i < sizeof(size_t); i++)
        {
            uint8_t outbyte = (dictout_size >> (8 * i)) & 0xFF;
            out.write(reinterpret_cast<char *>(&outbyte), sizeof(outbyte));
        }

        out.write(dictout.data(), dictout.size());

        out.write(reinterpret_cast<char *>(&word_width), sizeof(word_width));
        // printf("%2X,%2X,%2X,%2X\n", dictout[0], dictout[1], dictout[2], dictout[3]);

        /* Write encoded bytes to the file: */
        out.write(outbuf.data(), outbuf.size());

        printf("...DICT END\n");
        printf("%2X,%2X,%2X,%2X\n", dictout[dictout.size() - 4], dictout[dictout.size() - 3], dictout[dictout.size() - 2], dictout[dictout.size() - 1]);

        printf("ENCODED START...\n");
        printf("%2X,%2X,%2X,%2X\n", outbuf[0], outbuf[1], outbuf[2], outbuf[3]);

        std::cout << "Word width:" << static_cast<int>(word_width) << std::endl;
        std::cout << "Dict maxsize:" << dict.maxsize << std::endl;
        std::cout << "Dict count:" << dict.count << std::endl;
        /*
                std::vector<size_t> depths;
                dict.root->get_tree_stats(depths);

                std::ofstream csv(filename + ".depths_" + std::to_string(dict.maxsize) + ".csv");

                csv << dict.maxsize << std::endl;

                for (int i = 0; i < depths.size(); i++)
                {
                    csv << static_cast<long>(depths[i]) << std::endl;
                }

                csv.close(); */

        in.close();
        out.close();
    }
    else if (mode == "-d")
    {
        /* Decode the file: */
        const std::string out_filename = comp::common::trim_string_ext(filename);

        std::ifstream in(filename, std::ios::binary);
        // std::ofstream out(out_filename, std::ios::binary);

        if (!in)
        {
            std::cerr << "Error opening file " << filename << std::endl;
            return EXIT_FAILURE;
        }
        /* if (!out)
        {
            std::cerr << "Error opening file " << out_filename << std::endl;
            return EXIT_FAILURE;
        } */

        std::vector<uint8_t> decoded;

        uint8_t byte;

        size_t dict_bytecount = 0;

        for (int i = 0; i < sizeof(size_t); i++)
        {
            in.read(reinterpret_cast<char *>(&byte), sizeof(byte));
            dict_bytecount |= (static_cast<size_t>(byte) << (8 * i));
        }

        std::cout << "Dict bytecount: " << dict_bytecount << std::endl;

        std::vector<uint8_t> dict, seq;

        for (int i = 0; i < dict_bytecount; i++)
        {
            in.read(reinterpret_cast<char *>(&byte), sizeof(byte));
            dict.push_back(byte);
        }

        uint8_t word_width;
        in.read(reinterpret_cast<char *>(&word_width), sizeof(word_width));

        while (in.read(reinterpret_cast<char *>(&byte), sizeof(byte)))
        {
            seq.push_back(byte);
        }

        printf("DICT:\n");
        for (int i = 0; i < 8; i++)
        {
            printf("%02X ", dict[i]);
        }
        printf("\n...WW: %d...\n", word_width);

        for (int i = 0; i < 4; i++)
        {
            printf("%02X ", seq[i]);
        }
        printf("\nENCODED...\n");

        Dictionary d;
        size_t byte_count = 0;

        while (byte_count < dict_bytecount)
        {
            byte_count += d.build_from_stream(dict, word_width);
        }

        

        printf("\nBEGIN\n");

        for (const char c : decoded)
        {
            printf("%2X ", c);
        }
        printf("\n");

        /* Dump contents: */
        // out.write(reinterpret_cast<const char *>(decoded.data()), decoded.size());

        in.close();
        // out.close();
    }
    else
    {
        std::cout << "Usage: {-e|-d} <filename>" << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}