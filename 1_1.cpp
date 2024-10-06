/* Projekat 1, zadatak 1. */
#include <fstream>
#include <iostream>
#include <map>
#include <cstdint>
#include <cstdlib>
#include <cmath>

#include "common.hpp"

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cout << "Filename not provided" << std::endl;
        return EXIT_FAILURE;
    }

    const std::string filename(argv[1]);

    /* Initialized to 0 by default: */
    std::map<uint8_t, double> prob;

    comp::common::calc_prob(filename, prob);

    double entropy = 0.0;

    for(auto &[byte, pi]: prob) {
        if(pi) {
            entropy += (pi * log2(pi));
        }
    }

    entropy *= -1.0f;

    std::cout << "Entropy: " << entropy << std::endl;

    return EXIT_SUCCESS;
}