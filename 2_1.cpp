#include <iostream>
#include <cstdlib>
#include <cstdint>
#include <ctime>
#include <vector>

const uint8_t n = 15;
const uint8_t n_k = 9;

const uint8_t w_r = 5;
const uint8_t w_c = 3;

static_assert(n % w_r == 0, "Not divisible");

const uint8_t m = (n / w_r) * w_c;

uint8_t H[m][n];

const double th = 0.5;

void print_matrix()
{
    for (int i = 0; i < m; i++)
    {
        for (int j = 0; j < n; j++)
        {
            std::cout << static_cast<int>(H[i][j]) << " ";
        }
        std::cout << std::endl;
    }
}

void copy_column(int i_dest, int j_dest, int i_src, int j_src, int colspan)
{
    if (i_src + colspan > m || i_dest + colspan > m)
    {
        std::cerr << "Colsize exceeded" << std::endl;
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < colspan; i++)
    {
        H[i_dest + i][j_dest] = H[i_src + i][j_src];
    }
}

std::vector<int> gallager_b_decode(std::vector<int> received, int max_iterations = 10)
{
    int num_bits = received.size();
    int num_checks = m;

    std::vector<int> decoded = received;

    for (int iteration = 0; iteration < max_iterations; ++iteration)
    {
        std::vector<int> checks_count(num_bits, 0);

        for (int check = 0; check < num_checks; ++check)
        {
            int parity = 0;
            for (int bit = 0; bit < num_bits; ++bit)
            {
                if (H[check][bit] == 1)
                {
                    parity ^= decoded[bit];
                }
            }
            if (parity != 0)
            {
                for (int bit = 0; bit < num_bits; ++bit)
                {
                    if (H[check][bit] == 1)
                    {
                        checks_count[bit]++;
                    }
                }
            }
        }

        /* Threshold & bit flip:  */
        bool any_flipped = false;
        for (int bit = 0; bit < num_bits; ++bit)
        {
            if (checks_count[bit] > th * num_checks)
            {
                decoded[bit] ^= 1;
                any_flipped = true;
            }
        }

        /* Nothing left over to rectify, end: */
        if (!any_flipped)
        {
            break;
        }
    }

    return decoded;
}

int main(int argc, char *argv[])
{
    for (auto row : H)
    {
        for (uint8_t i; i < n; i++)
        {
            row[i] = 0;
        }
    }

    /*
    Write 1s:

    111........0
    0....111...0
    0........111
    ____________
    ...

    */
    for (uint8_t i = 0; i < n / w_r; i++)
    {
        for (uint8_t j = i * w_r; j < (i + 1) * w_r; j++)
        {
            H[i][j] = 1;
        }
    }

    /*     print_matrix();

        std::cout << "===================" << std::endl; */

    /* Seed: */
    srand(492018);

    for (int i = 1; i < w_c; i++)
    {
        uint8_t idx_shuffled[n];

        for (int j = 0; j < n; j++)
        {
            idx_shuffled[j] = j;
        }

        /* Permutations:
         0  1  2  3  4  5  6  7  8  9 10 11 12 13 14

                        |
                        | (rand)
                        v

         7 12  3 13  1  0 14 11  4  9  8  6  2 10  5
        */
        for (int j = 0; j < n * n; j++)
        {
            int p = rand() % n;
            int q = rand() % n;

            auto r = idx_shuffled[p];
            idx_shuffled[p] = idx_shuffled[q];
            idx_shuffled[q] = r;
        }

        for (int j = 0; j < n; j++)
        {
            int dest_row = i * (n / w_r);
            copy_column(dest_row, j, 0, idx_shuffled[j], n/w_r);
        }
    }

    /* Matrix H: */
    print_matrix();

    std::cout << "===================" << std::endl;

    std::vector<int> received = {1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1, 0};

    std::vector<int> decoded = gallager_b_decode(received);

    for (int bit : decoded)
    {
        std::cout << bit << " ";
    }
    std::cout << std::endl;

    return EXIT_SUCCESS;
}