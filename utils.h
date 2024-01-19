//
// Created by Guy Zyskind on 13/12/2022.
//

#ifndef DPFPIR_UTILS_H
#define DPFPIR_UTILS_H

#include <vector>
#include <cstdint>
#include <stdexcept>
#include <immintrin.h>
#include "Defines.h"
#include <fstream>
#include <filesystem>
#include <map>
#include <sstream>
#include <iostream>

const std::string DATA_DIR = "/home/azureuser/work/data/"; // TODO: generalize this
const int OTHER_PRIME = 4294967111;
const int PP = 2147483647;
const uint64_t MODINV2 = 1073741824;
const uint64_t MODINV3 = 1431655765;
const field MAX_VALID_INT = 536870912; // TODO: may want to change it when moving to 64bit..
const int N_THREADS = 4;
const int N_SPLITS = 16;

const std::map<std::string, int> ALL_BENCHMARKS = {
        {"DPF.Gen", 0},
        {"DPF.EvalAll", 1},
        {"ShamirDPF.Gen", 2},
        {"ShamirDPF.EvalAll", 3},
        {"VerShamirDPF.Gen", 4},
        {"VerShamirDPF.EvalAll", 5},
        {"ShamirDPFMulti.EvalAll", 6},
        {"FastDPF.EvalAll", 7},
        {"balance", 8},
        {"balanceMalicious", 9},
        {"transfer", 10},
        {"transferMalicious", 11},
        {"read", 12},
        {"write", 13}
};
const std::vector<std::string> BENCHMARK_NAMES = {
        "DPF.Gen",
        "DPF.EvalAll",
        "ShamirDPF.Gen",
        "ShamirDPF.EvalAll",
        "VerShamirDPF.Gen",
        "VerShamirDPF.EvalAll",
        "ShamirDPFMulti.EvalAll",
        "FastDPF.EvalAll",
        "balance",
        "balanceMalicious",
        "transfer",
        "transferMalicious",
        "read",
        "write"
};

// Some fake rands to test
const std::vector<std::vector<uint8_t>> XORRAND0 = {{73, 146, 219}, {57, 114, 75}, {66, 132, 198}, {248, 237, 21}, {2, 4, 6}, {7, 14, 9}, {35, 70, 101}, {1, 2, 3}, {218, 169, 115}, {202, 137, 67}, {190, 97, 223}, {148, 53, 161}, {164, 85, 241}, {142, 1, 143}, {42, 84, 126}, {174, 65, 239}};
const std::vector<std::vector<uint8_t>> XORRAND1 = {{115, 230, 149}, {8, 16, 24}, {70, 140, 202}, {48, 96, 80}, {36, 72, 108}, {123, 246, 141}, {183, 115, 196}, {35, 70, 101}, {231, 211, 52}, {111, 222, 177}, {166, 81, 247}, {218, 169, 115}, {182, 113, 199}, {194, 153, 91}, {147, 59, 168}, {221, 167, 122}};
const std::vector<std::vector<uint8_t>> XORRAND2 = {{110, 220, 178}, {69, 138, 207}, {94, 188, 226}, {146, 57, 171}, {37, 74, 111}, {128, 29, 157}, {33, 66, 99}, {85, 170, 255}, {10, 20, 30}, {10, 20, 30}, {35, 70, 101}, {237, 199, 42}, {123, 246, 141}, {140, 5, 137}, {166, 81, 247}, {9, 18, 27}};
const std::vector<std::vector<uint8_t>> XORRAND = {
{ 141, 195, 249 },
{ 159, 170, 185 },
{ 97, 211, 189 },
{ 204, 189, 146 },
{ 140, 241, 218 },
{ 3, 156, 233 },
{ 253, 59, 121 },
{ 233, 205, 209 },
{ 175, 144, 133 },
{ 19, 30, 238 },
{ 90, 218, 81 },
{ 39, 231, 167 },
{ 103, 178, 10 },
{ 238, 76, 217 },
{ 111, 130, 217 },
{ 171, 217, 247 }
};

struct RandData {
    std::vector<std::vector<int64_t>> xor_rands;
    std::vector<std::vector<int64_t>> xor_zeros;
    std::vector<std::vector<int64_t>> rands_degt;
    std::vector<std::vector<int64_t>> rands_deg2t;
    std::vector<std::vector<int64_t>> zeros_deg2t;
};

const block PP_block = _mm_set1_epi32(2147483647);
//const block PP2_block = _mm_set1_epi32(2147483648);
const block ONES_block = _mm_set1_epi32(1);

bool are_blocks_equal(const block& a, const block& b);
bool are_arrays_equal(const std::array<block, 4>& arr1, const std::array<block, 4>& arr2);
bool are_arrays_equal_2(const std::array<block, 2>& arr1, const std::array<block, 2>& arr2);
bool are_vectors_equal_2(const std::vector<block>& arr1, const std::vector<block>& arr2);

// mod 2^31 - 1
uint32_t modmersenne31(uint32_t x);
inline uint32_t modmersenne31safe64(uint64_t x) {
    uint64_t x0 = x >> 31;
    uint64_t x1 = x & PP;
    uint32_t res = x0 + x1;
    if (res == PP) return 0; // edge case;
    if (res > PP) {
        std::cout << res << " is still larger than the field size, running mod again" << std::endl;
        res = modmersenne31safe64(res); // This may occur if the original number was larger than 32bit?
    }
    return res;
}
inline int64_t mod(const int64_t a, const int64_t b) {
    int64_t result = a % b;
    return (result < 0) ? result + b : result;
}

inline uint64_t modu(const uint64_t a, const uint64_t b) {
    return a % b;
}

inline uint64_t vector_sum(__m512i a) {
    // Masks for high and low parts
    __m512i low_mask = _mm512_set1_epi64(0xFFFFFFFF);

    // Step 1: Split each vector into high and low 32-bit parts
    __m512i a_low = _mm512_and_si512(a, low_mask);
    __m512i a_high = _mm512_srli_epi64(a, 32);

    // Step 2: Sum each part separately
//    uint64_t sum_low = _mm512_reduce_add_epi64(a_low) % PP; // NOTE: I wonder if with a less favorable field this would work? reducing both the low bits and high bits
//    uint64_t sum_high = _mm512_reduce_add_epi64(a_high) % PP;
    uint64_t sum_low = _mm512_reduce_add_epi64(a_low);
//    uint64_t sum_low = modmersenne31safe64(_mm512_reduce_add_epi64(a_low));
    uint64_t sum_high = modmersenne31safe64(_mm512_reduce_add_epi64(a_high));

    // Step 3: Combine the sums with proper handling of carry
    uint64_t carry = (sum_low >> 32);
    sum_low &= 0xFFFFFFFF;  // Ensure sum_low is within 32-bit range
    sum_high = (sum_high + carry); // Add carry to sum_high

    // Combine the sums for the final result
    uint64_t result = (sum_high << 32) | sum_low;

    return result;  // Apply modulo to the final result
}

//inline uint64_t vector_sum(__m512i a) {
//    // Masks for high and low parts
//    __m512i low_mask = _mm512_set1_epi64(0xFFFFFFFF);
//
//    // Step 1: Split each vector into high and low 32-bit parts
//    __m512i a_low = _mm512_and_si512(a, low_mask);
//    __m512i a_high = _mm512_srli_epi64(a, 32);
//
//    // Step 2: Sum each part
//    uint64_t sum_low = _mm512_reduce_add_epi64(a_low);
//    uint64_t sum_high = _mm512_reduce_add_epi64(a_high);
//
//    // Step 3: Adjust for carry from low sum and combine the sums
//    uint64_t carry = (sum_low >> 32);
//    uint64_t adjusted_sum_high = sum_high + carry;
//    uint64_t result = (adjusted_sum_high << 32) | (sum_low & 0xFFFFFFFF);
//
//    return result;
//}

inline uint64_t vector_sum2(__m512i a) {
    // Sum 64-bit integers directly, as they are the result of 32-bit multiplications extended to 64 bits.
    return _mm512_reduce_add_epi64(a) % PP;
//    return _mm512_reduce_add_epi64(a);
}

inline uint64_t vector_sum3(__m512i a) {
    uint64_t result = 0;
    alignas(64) uint64_t temp[8]; // Temporary array to store the 64-bit integers from the vector

    // Store the 64-bit integers from the vector into the temporary array
    _mm512_store_si512(reinterpret_cast<__m512i*>(temp), a);

    // Sum the elements of the temporary array
    for (int i = 0; i < 8; ++i) {
        result = (result + temp[i]) % PP;
    }

    // Apply modulo operation
    result %= PP;

    return result;
}


// vectorized (4 ints) mod 2^31 - 1
block modmersenne31block(block x);

int print_fake_zero_triplets_code();
uint32_t convertToUint32(const std::vector<uint8_t>& array);
std::vector<uint8_t> convertToUint8Vector(uint32_t value, size_t total_bytes);
void printVector(std::vector<uint8_t> vec);
std::vector<std::pair<uint8_t, uint8_t>> fake_xor_rand(int idx);
void print_fake_block_sharing();
std::vector<std::uint32_t> detrandints(int n_size, int p, const unsigned int seed = 123);
RandData generate_random_sharings(int n_size, int p, const unsigned int seed);
void saveToFile(const std::vector<uint32_t>& data, const std::string& filename);

#endif //DPFPIR_UTILS_H
