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
    if (argc < 2)
    {
        std::cout << "Filename not provided" << std::endl;
        return EXIT_FAILURE;
    }

    const std::string filename(argv[1]);

    comp::common::shannon_fano_encode(filename);


    return EXIT_SUCCESS;
}
