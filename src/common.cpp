#include "common.hpp"

#include <fstream>
#include <iostream>
#include <map>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <vector>
#include <memory>
#include <iomanip>

const std::string comp::common::sf_ext = ".sfef";

struct comp::common::_sf_data
{
    uint8_t byte;
    double prob;
    std::vector<bool> prefix;
};

/* Trie */
/* Example: how two prefixes 001011 and 0011 are stored in the same trie: */

/*
 0        false
 0        false
 1\       true\
 0 1  ->  false TRUE
 1        true
 1        TRUE

 Leaf nodes are set in all-caps, and hold the corresponding byte.
 */
struct comp::common::_node
{
    std::map<bool, std::unique_ptr<struct comp::common::_node>> nodes;

    bool ends = false;
    uint8_t decoded_byte;

    /* TODO: don't modify bits */
    void insert(std::vector<bool> &bits, uint8_t byte)
    {
        // TODO: .empty()?
        if (bits.size() == 0)
        {
            decoded_byte = byte;
            ends = true;
            return;
        }

        const bool next = bits[0];
        bits.erase(bits.begin());

        /* Insert if the bit does not exist: */
        if (nodes.find(next) == nodes.end())
        {
            nodes[next] = std::make_unique<struct comp::common::_node>();
        }
        /* Recursively traverse and keep adding nodes where needed: */
        nodes[next]->insert(bits, byte);
    }

    comp::common::_node *find(uint8_t byte, uint8_t &leftover_bits)
    {
        if ((leftover_bits == 0) || ends)
        {
            return this;
        }

        const bool next = (byte & (0x1 << (leftover_bits - 1))) ? true : false;
        if (nodes.find(next) == nodes.end())
        {
            return nullptr;
        }

        leftover_bits--;
        return nodes[next]->find(byte, leftover_bits);
    }

    static std::string byteToHex(uint8_t byte)
    {
        std::ostringstream oss;
        oss << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
        return oss.str();
    }

    static void printTrie(const _node *node, const std::string &prefix = "")
    {
        if (!node)
            return;

        // Print current node details
        std::cout << prefix;
        if (node->ends)
        {
            std::cout << " [END] Decoded Byte: " << byteToHex(node->decoded_byte);
        }
        std::cout << std::endl;

        // Recur for true and false branches
        if (node->nodes.find(false) != node->nodes.end())
        {
            std::cout << prefix << "0 -> ";
            printTrie(node->nodes.at(false).get(), prefix + "  ");
        }

        if (node->nodes.find(true) != node->nodes.end())
        {
            std::cout << prefix << "1 -> ";
            printTrie(node->nodes.at(true).get(), prefix + "  ");
        }
    }
};

std::string trim_string_ext(const std::string& str) {
    std::size_t lastDot = str.find_last_of('.');

    if (lastDot != std::string::npos && lastDot != 0) {
        return str.substr(0, lastDot);
    }

    return str;
}

void comp::common::shannon_fano_decode(const std::string &filename)
{
    std::ifstream in(filename, std::ios::binary);
    std::ofstream out(trim_string_ext(filename), std::ios::binary);
    std::cout << trim_string_ext(filename) << std::endl;
    // TODO auto
    auto trie = std::make_unique<_node>();

    uint8_t valid_bits;
    uint8_t byte;

    /* File header (prefixes): */
    for (int i = 0; i < 256; i++)
    {
        in.read(reinterpret_cast<char *>(&valid_bits), sizeof valid_bits);

        const bool excess_bits = (valid_bits % 8) > 0;
        const uint8_t bytes_to_read = valid_bits / 8 + (excess_bits ? 1 : 0);

        std::vector<bool> bits;
        std::vector<uint8_t> bytes;

        for (int j = 0; j < bytes_to_read; j++)
        {
            in.read(reinterpret_cast<char *>(&byte), sizeof byte);
            bytes.push_back(byte);
        }

        uint8_t mask = 0x80;
        for (int j = 0; j < valid_bits; j++)
        {
            bits.push_back((bytes[j / 8] & mask) ? true : false);
            mask >>= 1;

            if (mask == 0x0)
            {
                mask = 0x80;
            }
        }

        /* printf("%5d: ", i);
        for(int j = 0; j < bits.size(); j++) {
            printf("%c", (bits[j]) ? '1' : '0');
        }
        printf("\n"); */
        trie->insert(bits, static_cast<uint8_t>(i));

    }

    uint8_t leftover = 0;
    _node *t = trie.get();

    for (;;)
    {
        if (leftover == 0)
        {
            if (!(in.read(reinterpret_cast<char *>(&byte), sizeof byte)))
            {
                // TODO check excess (padded) bits handling at the end of the file
                goto cleanup;
            }
            leftover = 8;
        }

        t = t->find(byte, leftover);
        if (t == NULL)
        {
            std::cout << "Fatal error: " << __LINE__ << std::endl;
        }

        if (t->ends)
        {
            out.write(reinterpret_cast<char*>(&(t->decoded_byte)), sizeof t->decoded_byte);
            t = trie.get();
        }
    }

cleanup:
    in.close();
    out.close();
}

void comp::common::shannon_fano_encode(const std::string &filename)
{
    /* Initialized to 0 by default: */
    std::map<uint8_t, double> prob;
    std::vector<std::pair<uint8_t, std::vector<bool>>> result;

    comp::common::calc_prob(filename, prob);

    /*     for (auto &t : prob)
        {
            fprintf(stdout, "%5d: %f\n", t.first, t.second);
            printf("\n");
        } */

    /* Convert to a vector, for sorting: */
    std::vector<std::pair<uint8_t, double>> vec(prob.begin(), prob.end());

    struct
    {
        bool operator()(const std::pair<uint8_t, double> &a, const std::pair<uint8_t, double> &b)
        {
            return a.second > b.second;
        };
    } cmpPair;

    std::sort(vec.begin(), vec.end(), cmpPair);
    std::vector<struct _sf_data> dat;

    for (std::pair<uint8_t, double> &t : vec)
    {
        dat.push_back((_sf_data){.byte = t.first, .prob = t.second});
    }

    uint8_t l = 0;
    uint8_t r = vec.size() - 1;

    comp::common::_shannon_fano(dat, l, r, 1);

    for (_sf_data &t : dat)
    {
        result.push_back(std::pair<uint8_t, std::vector<bool>>(t.byte, t.prefix));
    }

    std::sort(result.begin(), result.end(), std::less<std::pair<double, std::vector<bool>>>());

    /*     for (auto &t : result)
        {
            fprintf(stdout, "%5d: ", t.first);
            for (int i = 0; i < t.second.size(); i++)
            {
                printf("%c", (t.second[i]) ? '1' : '0');
            }
            printf("\n");
        } */

    const std::string output_filename = filename + comp::common::sf_ext;

    std::ofstream out(output_filename, std::ios::binary);

    /*
     * Serialize into output_filename:
     * 1. { prefix bit count | prefix + padding (mod 8 == 0) }, n times, n ASC
     * 2. { encoded contents }
     */

    /* 1. */
    for (auto &[b, vec] : result)
    {
        uint8_t prefix_bit_count = static_cast<uint8_t>(vec.size());
        out.write(reinterpret_cast<char *>(&prefix_bit_count), sizeof prefix_bit_count);

        // TODO: Fix endianness issue!S!
        uint8_t padded_prefix[256 / (sizeof(uint8_t) * CHAR_BIT)];
        if (vec.size() == 0)
        {
            std::cout << "Fatal error: " << __LINE__ << std::endl;
            exit(EXIT_FAILURE);
        }
        uint8_t len_mod_8 = (((vec.size() - 1) / CHAR_BIT) + 1) * CHAR_BIT;

        int i;
        const uint8_t mask = 0x1;
        uint8_t byte = 0x0;

        // When byte resets to 0x0, subsequent bit shift has no ill effect
        for (i = 0; i < len_mod_8; i++, byte = byte << 1)
        {
            if (i < vec.size() && vec[i])
            {
                byte = byte | mask;
            }
            if ((i + 1) % 8 == 0)
            {
                out.write(reinterpret_cast<char *>(&byte), sizeof byte);
                byte = 0x0;
            }
        }
    }

    /* 2. */
    std::ifstream in(filename, std::ios::binary);

    if (!in.is_open())
    {
        std::cout << "Failed to open: " << filename << std::endl;
        exit(EXIT_FAILURE);
    }

    std::map<uint8_t, std::vector<bool>> bitmap(result.begin(), result.end());

    uint8_t in_byte;
    uint8_t out_byte;
    uint8_t out_byte_bit_count = 0;

    while (in.read(reinterpret_cast<char *>(&in_byte), sizeof in_byte))
    {
        std::vector<bool> prefix_bits = bitmap[in_byte];

        for (bool pb : prefix_bits)
        {
            const uint8_t write_mask = (pb) ? 0x1 : 0x0;
            out_byte = out_byte | write_mask;
            out_byte_bit_count++;

            if (out_byte_bit_count == 8)
            {
                out.write(reinterpret_cast<char *>(&out_byte), sizeof out_byte);
                out_byte_bit_count = 0;
                out_byte = 0;
            }
            else
            {
                out_byte <<= 1;
            }
        }
    }

    /* Flush any remaining bits: */
    if (out_byte_bit_count)
    {
        out_byte <<= (8 - out_byte_bit_count);
        out.write(reinterpret_cast<char *>(&out_byte), sizeof out_byte);
    }

    in.close();
    out.close();
}

/* Split + build prefix */
void comp::common::_shannon_fano(std::vector<struct _sf_data> &dat, uint8_t p, uint8_t q, double s)
{
    if (p >= q)
    {
        return;
    }

    uint8_t lim = p;

    double sum = dat[p].prob;

    /* lim + 1 at worst would still be within bounds */
    while (lim < q && (fabs(sum + dat[lim + 1].prob - (s / 2)) < fabs(sum - (s / 2))))
    {
        lim++;
        sum += dat[lim].prob;
    }
    //    std::cout << static_cast<int>(p) << " [" << static_cast<int>(lim) << "] " << static_cast<int>(q) << " | " << sum << std::endl;

    int i;
    for (i = p; i <= lim; i++)
    {
        dat[i].prefix.push_back(false);
    }

    for (; i <= q; i++)
    {
        dat[i].prefix.push_back(true);
    }

    comp::common::_shannon_fano(dat, p, lim, sum);
    comp::common::_shannon_fano(dat, lim + 1, q, s - sum);
}

void comp::common::calc_prob(std::string filename, std::map<uint8_t, double> &prob)
{
    std::map<uint8_t, int> bytes;
    std::ifstream in(filename, std::ios::binary);

    if (!in.is_open())
    {
        std::cout << "Failed to open: " << filename << std::endl;
        exit(EXIT_FAILURE);
    }

    uint8_t t;
    uint64_t total = 0;

    while (in.read(reinterpret_cast<char *>(&t), sizeof t))
    {
        bytes[t]++;
        total++;
    }

    in.close();

    for (auto &[byte, count] : bytes)
    {
        prob[byte] = static_cast<double>(count) / total;
    }
}