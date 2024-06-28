#include "common.hpp"

#include <fstream>
#include <iostream>
#include <map>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <vector>

const std::string comp::common::sf_ext = ".sfef";

struct comp::common::_sf_data
{
    uint8_t byte;
    double prob;
    std::vector<bool> prefix;
};

void comp::common::shannon_fano_decode(const std::string &filename)
{
    
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

    for (auto &t : result)
    {
        fprintf(stdout, "%5d: ", t.first);
        for (int i = 0; i < t.second.size(); i++)
        {
            printf("%c", (t.second[i]) ? '1' : '0');
        }
        printf("\n");
    }

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