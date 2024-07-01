#ifndef COMMON_HPP
#define COMMON_HPP

#include <map>
#include <string>
#include <cstdint>
#include <vector>
#include <any>
#include <memory>

namespace comp
{
    class common
    {
    public:
        static void calc_prob(std::string, std::map<uint8_t, double> &);
        static void shannon_fano_encode(const std::string &);
        static void huffman_encode(const std::string &);
        static void decode(const std::string &);

        static const std::string sf_ext;
        static const std::string hf_ext;

    private:
        struct _node;
        struct _sf_data;
        static void _write_encoded(std::vector<std::pair<uint8_t, std::vector<bool>>> &, const std::string &, const std::string &);
        static void _shannon_fano(std::vector<struct _sf_data> &, uint8_t, uint8_t, double);
        static void _huffman_code_gen(std::shared_ptr<comp::common::_node> &, std::vector<bool> &, std::map<uint8_t, std::vector<bool>> &);
        static std::shared_ptr<comp::common::_node> join_nodes(std::shared_ptr<comp::common::_node>, std::shared_ptr<comp::common::_node>);
    };
}

#endif