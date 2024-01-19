//
// Created by Guy Zyskind on 08/12/2022.
//

#include "pirw.h"
#include "utils.h"
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
    void addvff31(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b, std::vector<uint32_t>& c)
    {
        // Make sure the vectors have the same size
        if (a.size() != b.size())
        {
            throw std::invalid_argument("Vectors must have the same size");
        }

        // Create a vector to hold the result

        // Add the corresponding elements of a and b and store the result in c
//#pragma omp parallel for simd num_threads(N_THREADS)
//#pragma omp simd
        for (size_t i = 0; i < a.size(); i++)
        {
//            c[i] = modmersenne31(a[i] + b[i]);
//            c[i] = (a[i] + b[i]) % PP;
            c[i] = mod((static_cast<int64_t>(a[i]) + b[i]), PP);
        }

    }

    // Takes two vectors a, b, and returns a vector c which subtracts them together
    void subvff31(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b, std::vector<uint32_t>& c)
    {
        // Make sure the vectors have the same size
        if (a.size() != b.size())
        {
            throw std::invalid_argument("Vectors must have the same size");
        }

        // Add the corresponding elements of a and b and store the result in c
//#pragma omp parallel for simd num_threads(N_THREADS)
//#pragma omp simd
        for (size_t i = 0; i < a.size(); i++)
        {
//            c[i] = modmersenne31(a[i] + b[i]);
//            c[i] = (a[i] - b[i]) % PP;
            c[i] = mod((static_cast<int64_t>(a[i]) - b[i]), PP);
        }
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
//        #pragma omp parallel for reduction(+:inner_product) num_threads(N_THREADS)
//#pragma omp simd for reduction(+:inner_product)
        for (size_t i = 0; i < a.size(); i++)
        {
            uint64_t tmp;

            tmp = (static_cast<uint64_t>(a[i]) * b[i]) % PP;

//            if ((tmp & mask) != 0) {
//                tmp %= PP;
//            } else if ((inner_product & mask2) != 0) {
//                inner_product %= PP;
//            }

            inner_product += tmp;
        }

        return inner_product % PP;
    }

    uint32_t innerprodff31v(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
        if (a.size() != b.size()) {
            throw std::invalid_argument("Vectors must have the same size");
        }

        uint64_t inner_product = 0;

        for (size_t i = 0; i < a.size(); i += 8) {
            __m256i vec_a256 = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(&a[i]));
            __m256i vec_b256 = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(&b[i]));
//            __m256i vec_a256 = _mm256_load_si256(reinterpret_cast<const __m256i *>(&a[i]));
//            __m256i vec_b256 = _mm256_load_si256(reinterpret_cast<const __m256i *>(&b[i]));
            __m512i vec_a = _mm512_cvtepu32_epi64(vec_a256); // Zero extend to be 64-bits
            __m512i vec_b = _mm512_cvtepu32_epi64(vec_b256);

//            __m512i vec_c = _mm512_mullox_epi64(vec_a, vec_b); // This intrinsic generates a sequence of instructions, which may perform worse than a native instruction. Consider the performance impact of this intrinsic.
            __m512i vec_c = _mm512_mul_epu32(vec_a, vec_b);

//            inner_product = modu(inner_product + vector_sum(vec_c), PP);
            inner_product = (inner_product + vector_sum(vec_c)) % PP;
        }

        return inner_product % PP;
    }

    // Sums a vector
    uint32_t sumvecff31(const std::vector<uint32_t>& a)
    {
        uint64_t result = 0;

        // Compute the inner product of the two vectors by summing the
        // products of the corresponding elements of a and b
//        #pragma omp parallel for reduction(+:result) num_threads(N_THREADS)
//#pragma omp simd for reduction(+:result)
        for (size_t i = 0; i < a.size(); i++)
        {
            result += a[i];
        }

//        return result % PP;
        return mod(result, PP);
//        return modmersenne31(inner_product);
    }


} // PIRW