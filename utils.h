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

const int PP = 2147483647;

const block PP_block = _mm_set1_epi32(2147483647);
const block PP2_block = _mm_set1_epi32(2147483648);

// mod 2^31 - 1
uint32_t modmersenne31(uint32_t x);
uint32_t modmersenne31safe64(uint64_t x); // hackish to prevent overflow when multiplying two 32-bit numbers

// vectorized (4 ints) mod 2^31 - 1
block modmersenne31block(block x);

#endif //DPFPIR_UTILS_H
