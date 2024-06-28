#ifndef COMMON_HPP
#define COMMON_HPP

#include <map>
#include <string>
#include <cstdint>
#include <vector>

namespace comp
{
    class common
    {
    public:
        static void calc_prob(std::string, std::map<uint8_t, double> &);
        static void shannon_fano_encode(const std::string&);
        static void shannon_fano_decode(const std::string&);
        static const std::string sf_ext;
    private:
        struct _node;
        struct _sf_data;
        static void _shannon_fano(std::vector<struct _sf_data> &, uint8_t, uint8_t, double);
    };
}

#endif