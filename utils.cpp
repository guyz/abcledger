//
// Created by Guy Zyskind on 13/12/2022.
//

#include "utils.h"
#include <vector>
#include <cstdint>
#include <stdexcept>
#include "Defines.h"
#include <cassert>
#include <iostream>
#include <emmintrin.h>

bool are_blocks_equal(const block& a, const block& b) {
    // Compare 128-bit blocks by treating them as an array of integers
    for (int i = 0; i < sizeof(block) / sizeof(uint32_t); ++i) {
        if (((uint32_t*)&a)[i] != ((uint32_t*)&b)[i]) {
            return false;
        }
    }
    return true;
}

bool are_arrays_equal(const std::array<block, 4>& arr1, const std::array<block, 4>& arr2) {
    for (size_t i = 0; i < arr1.size(); ++i) {
        if (!are_blocks_equal(arr1[i], arr2[i])) {
            return false;
        }
    }
    return true;
}

bool are_arrays_equal_2(const std::array<block, 2>& arr1, const std::array<block, 2>& arr2) {
    for (size_t i = 0; i < arr1.size(); ++i) {
        if (!are_blocks_equal(arr1[i], arr2[i])) {
            return false;
        }
    }
    return true;
}

bool are_vectors_equal_2(const std::vector<block>& arr1, const std::vector<block>& arr2) {
    for (size_t i = 0; i < arr1.size(); ++i) {
        if (!are_blocks_equal(arr1[i], arr2[i])) {
            return false;
        }
    }
    return true;
}

uint32_t modmersenne31(uint32_t x) {
    uint32_t x0 = x >> 31;
    uint32_t x1 = x & PP;
    uint32_t res = x0 + x1;
    if (res == PP) return 0; // edge case;
    return res;
}

uint32_t modmersenne31safe64(uint64_t x) {
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

// Returns the 'positive mod' - i.e., what we need to operate over a field.
// Only relevant when computing minus or sub..
int mod(int a, int b) {
    int result = a % b;
    if (result < 0) {
        result += b;
    }
    return result;
}


block modmersenne31block(block x) {
    // vectorized
    block x0 = _mm_srli_epi32(x, 31);
    block x1 = _mm_and_si128(x, PP_block);
    return _mm_add_epi32(x0, x1);
    // end vectorized

//
//    reg_arr_union tmp = {ZeroBlock};
//    reg_arr_union x0 = {ZeroBlock};
//    reg_arr_union res = {ZeroBlock};
////    reg_arr_union x1 = {ZeroBlock};
////    x1.reg = _mm_and_si128(x, PP_block);
//    tmp.reg = x;
//    x0.reg = _mm_set_epi32(tmp.arr32[3] >> 31, tmp.arr32[2] >> 31, tmp.arr32[1] >> 31, tmp.arr32[0] >> 31); // TODO: fix this? Because I'm getting signed numbers in vectorized AND op?
//
//    res.reg = _mm_add_epi32(x0.reg, x1);
//
//    // TODO: remove this later
////    for (int i = 0; i < 4; i++) {
////        if (res.arr32[i] == PP) res.arr32[i] = 0;
////        assert(res.arr32[i] == (tmp.arr32[i] % PP) );
////    }
//    // END REMOVE
//
//    return res.reg;
}