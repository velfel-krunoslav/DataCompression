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
#include <queue>
#include <any>

const std::string comp::common::sf_ext = ".sfef";
const std::string comp::common::hf_ext = ".hfef";

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
struct comp::common::_node : public std::enable_shared_from_this<_node>
{
    std::shared_ptr<comp::common::_node> get_shared()
    {
        return shared_from_this();
    };

    std::map<bool, std::shared_ptr<struct comp::common::_node>> nodes;

    bool ends = false;
    std::any data;

    /* TODO: don't modify bits */
    std::shared_ptr<comp::common::_node> insert(std::vector<bool> &bits, std::any dat)
    {
        // TODO: .empty()?
        if (bits.empty())
        {
            data = dat;
            ends = true;
            return get_shared();
        }

        const bool next = bits[0];
        bits.erase(bits.begin());

        /* Insert if the bit does not exist: */
        if (nodes.find(next) == nodes.end())
        {
            nodes[next] = std::make_shared<struct comp::common::_node>();
        }
        /* Recursively traverse and keep adding nodes where needed: */
        return nodes[next]->insert(bits, dat);
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
};

std::string  comp::common::trim_string_ext(const std::string &str)
{
    std::size_t lastDot = str.find_last_of('.');

    if (lastDot != std::string::npos && lastDot != 0)
    {
        return str.substr(0, lastDot);
    }

    return str;
}

void comp::common::_write_encoded(std::vector<std::pair<uint8_t, std::vector<bool>>> &result,
                                  const std::string &filename, const std::string &output_filename)
{
    std::ifstream in(filename, std::ios::binary);
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

std::shared_ptr<comp::common::_node> comp::common::join_nodes(std::shared_ptr<comp::common::_node> left, std::shared_ptr<comp::common::_node> right)
{
    auto tmp = std::make_shared<comp::common::_node>();
    tmp->nodes[false] = right;
    tmp->nodes[true] = left;

    /*     auto left_prob = std::any_cast<std::pair<std::vector<uint8_t>, double>>(left->data).second;
        auto right_prob = std::any_cast<std::pair<std::vector<uint8_t>, double>>(right->data).second;

        if (right_prob < left_prob) {
            std::swap(tmp->nodes[false], tmp->nodes[true]);
        } */

    return tmp;
}

void comp::common::_huffman_code_gen(std::shared_ptr<comp::common::_node> &node, std::vector<bool> &prefix_buffer, std::map<uint8_t, std::vector<bool>> &prefixes)
{
    if (std::any_cast<std::pair<std::vector<uint8_t>, double>>(node->data).first.size() == 1)
    {
        auto byte = std::any_cast<std::pair<std::vector<uint8_t>, double>>(node->data).first[0];
        prefixes[byte] = prefix_buffer;
        return;
    }

    prefix_buffer.push_back(false);
    _huffman_code_gen(node->nodes[false], prefix_buffer, prefixes);
    prefix_buffer.pop_back();

    prefix_buffer.push_back(true);
    _huffman_code_gen(node->nodes[true], prefix_buffer, prefixes);
    prefix_buffer.pop_back();
}

void comp::common::huffman_encode(const std::string &filename)
{
    std::map<uint8_t, double> prob;
    comp::common::calc_prob(filename, prob);

    struct cmpPair
    {
        bool operator()(const std::shared_ptr<_node> &a, const std::shared_ptr<_node> &b) const
        {
            auto first = std::any_cast<std::pair<std::vector<uint8_t>, double>>(a->data);
            auto second = std::any_cast<std::pair<std::vector<uint8_t>, double>>(b->data);

            return second.second < first.second;
        }
    } cmp;

    std::priority_queue<std::shared_ptr<_node>, std::vector<std::shared_ptr<_node>>, cmpPair> result;

    for (auto &[byte, pi] : prob)
    {
        auto new_node = std::make_shared<_node>();
        new_node->data = std::make_pair(std::vector<uint8_t>{byte}, pi);
        result.push(new_node);
    }

    while (result.size() != 1)
    {
        std::shared_ptr<_node> data[2];

        for (int i = 0; i < 2; i++)
        {
            data[i] = result.top();
            result.pop();
        }

        /* Flip order: */
        auto new_node = comp::common::join_nodes(data[0], data[1]);

        auto left_data = std::any_cast<std::pair<std::vector<uint8_t>, double>>(data[0]->data);
        auto right_data = std::any_cast<std::pair<std::vector<uint8_t>, double>>(data[1]->data);

        std::vector<uint8_t> tmp;
        tmp.reserve(left_data.first.size() + right_data.first.size());

        tmp.insert(tmp.end(), left_data.first.begin(), left_data.first.end());
        tmp.insert(tmp.end(), right_data.first.begin(), right_data.first.end());

        new_node->data = std::make_pair(tmp, left_data.second + right_data.second);
        result.push(new_node);
    }

    auto root = result.top();

    std::map<uint8_t, std::vector<bool>> codes;
    std::vector<bool> buf;

    auto tree = std::make_shared<comp::common::_node>();
    _huffman_code_gen(root, buf, codes);

    std::vector<std::pair<uint8_t, std::vector<bool>>> _result(codes.begin(), codes.end());

    const std::string output_filename = filename + hf_ext;

    _write_encoded(_result, filename, output_filename);
}

void comp::common::decode(const std::string &filename)
{
    // TODO what if the decoded filename already exists?
    std::ifstream in(filename, std::ios::binary);
    std::ofstream out(trim_string_ext(filename), std::ios::binary);
    // TODO auto
    auto trie = std::make_shared<_node>();

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
        auto ptr = trie->insert(bits, static_cast<uint8_t>(i));
    }

    _node *t = trie.get();
    uint8_t leftover = 0;
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
            auto decoded = std::any_cast<uint8_t>(t->data);
            out.write(reinterpret_cast<char *>(&(decoded)), sizeof decoded);
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

    for (_sf_data t : dat)
    {
        result.push_back(std::pair<uint8_t, std::vector<bool>>(t.byte, t.prefix));
    }

    std::sort(result.begin(), result.end(), std::less<std::pair<double, std::vector<bool>>>());

    const std::string output_filename = filename + comp::common::sf_ext;

    _write_encoded(result, filename, output_filename);
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