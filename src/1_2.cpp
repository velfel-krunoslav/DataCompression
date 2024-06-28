/* Projekat 1, zadatak 2 */

#include <fstream>
#include <iostream>
#include <map>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <sstream>

#include "common.hpp"

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        std::cout << "Filename not provided" << std::endl;
        return EXIT_FAILURE;
    }

    const std::string filename(argv[2]);

    if (std::string(argv[1]) == "-d")
    {
        comp::common::shannon_fano_decode(filename);
    }
    else if (std::string(argv[1]) == "-e")
    {
        comp::common::shannon_fano_encode(filename);
    }
    else
    {
        std::cout << "Unknown option" << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
