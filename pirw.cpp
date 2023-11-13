//
// Created by Guy Zyskind on 08/12/2022.
//

#include "pirw.h"

#include <vector>
#include <cstdint>
#include <stdexcept>

namespace PIRW {

    const int PP = 2147483647;

    int modmersenne31(uint64_t x) {
        int x0 = x >> 31;
        int x1 = x & PP;
        return x0 + x1;
    }

    // Takes two vectors a, b, and returns a vector c which adds them together
    std::vector<uint32_t> addvff31(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b)
    {
        // Make sure the vectors have the same size
        if (a.size() != b.size())
        {
            throw std::invalid_argument("Vectors must have the same size");
        }

        // Create a vector to hold the result
        std::vector<uint32_t> c(a.size());

        // Add the corresponding elements of a and b and store the result in c
        for (size_t i = 0; i < a.size(); i++)
        {
            c[i] = modmersenne31(a[i] + b[i]);
        }

        return c;
    }

    // Takes two vectors a, b, and returns a scalar c which is the inner product of the two
    uint32_t innerprodff31(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b)
    {
        // Make sure the vectors have the same size
        if (a.size() != b.size())
        {
            throw std::invalid_argument("Vectors must have the same size");
        }

        // Initialize the inner product to zero
        uint64_t inner_product = 0;

        // Compute the inner product of the two vectors by summing the
        // products of the corresponding elements of a and b
        #pragma omp parallel for reduction(+:inner_product) num_threads(16)
        for (size_t i = 0; i < a.size(); i++)
        {
            uint64_t tmp = modmersenne31(static_cast<int64_t>(a[i]) * b[i]);
            inner_product += tmp;
        }

        return modmersenne31(inner_product);
    }


} // PIRW