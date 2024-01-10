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

const std::string DATA_DIR = "/home/azureuser/data/";
const int OTHER_PRIME = 4294967111;
const int PP = 2147483647;
const uint64_t MODINV2 = 1073741824;
const uint64_t MODINV3 = 1431655765;
const field MAX_VALID_INT = 536870912; // TODO: may want to change it when moving to 64bit..

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
uint32_t modmersenne31safe64(uint64_t x); // hackish to prevent overflow when multiplying two 32-bit numbers
int64_t mod(int64_t a, int64_t b);

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
//uint8_t get_random_xor_share(int db_index, int server_index);
//int64_t get_random_degt_share(int db_index, int server_index);
//int64_t get_random_deg2t_share(int db_index, int server_index);

#endif //DPFPIR_UTILS_H
