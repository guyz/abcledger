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

const block PP_block = _mm_set1_epi32(2147483647);
//const block PP2_block = _mm_set1_epi32(2147483648);
const block ONES_block = _mm_set1_epi32(1);

// mod 2^31 - 1
uint32_t modmersenne31(uint32_t x);
uint32_t modmersenne31safe64(uint64_t x); // hackish to prevent overflow when multiplying two 32-bit numbers
int mod(int a, int b);

// vectorized (4 ints) mod 2^31 - 1
block modmersenne31block(block x);

#endif //DPFPIR_UTILS_H
