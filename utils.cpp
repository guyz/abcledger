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
#include "shamir.h"
#include <random>

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

std::vector<uint8_t> gen_zero_sharing() {
    auto shares = share_gf256(0, 3, 1);
    assert(reconstruct_gf256({shares[0], shares[1]}) == 0);
    return {
        shares[0].second,
        shares[1].second,
        shares[2].second
    };
}

int print_fake_zero_triplets_code() {
    const int numMainVectors = 3;
    const int numSubVectors = 16;
    const int numElements = 3;

    generate_tables();

    // Initialize main vectors
    std::vector<std::vector<uint8_t>> XORRAND0(numSubVectors, std::vector<uint8_t>(numElements));
    std::vector<std::vector<uint8_t>> XORRAND1(numSubVectors, std::vector<uint8_t>(numElements));
    std::vector<std::vector<uint8_t>> XORRAND2(numSubVectors, std::vector<uint8_t>(numElements));

    // Populate the vectors
    for (int i = 0; i < numSubVectors; ++i) {
        XORRAND0[i] = gen_zero_sharing();
        XORRAND1[i] = gen_zero_sharing();
        XORRAND2[i] = gen_zero_sharing();
    }

    // Print out the CPP code for static initialization
    std::vector<std::vector<uint8_t>>* XORRANDS[numMainVectors] = {&XORRAND0, &XORRAND1, &XORRAND2};
    for (int i = 0; i < numMainVectors; ++i) {
        std::cout << "std::vector<std::vector<uint8_t>> XORRAND" << i << " = {";
        for (int j = 0; j < numSubVectors; ++j) {
            std::cout << "{";
            for (int k = 0; k < numElements; ++k) {
                std::cout << static_cast<int>((*XORRANDS[i])[j][k]);
                if (k < numElements - 1) std::cout << ", ";
            }
            std::cout << "}";
            if (j < numSubVectors - 1) std::cout << ", ";
        }
        std::cout << "};" << std::endl;
    }

    return 0;
}

std::vector<std::pair<uint8_t, uint8_t>> fake_xor_rand(int idx) {
    std::vector<std::pair<uint8_t, uint8_t>> res;
    for (int i=0; i<16; i++) {
        res.push_back(std::make_pair(idx + 1, XORRAND[i][idx]));
    }

    return res;
}

void print_fake_block_sharing() {
    std::random_device rd; // obtain a random number from hardware
    std::mt19937 gen(rd()); // seed the generator
    std::uniform_int_distribution<> distr(0, 255); // define the range
    std::vector<uint8_t> r;
    std::cout << "r: ";
    for (int n=0; n<16; ++n) {
        uint8_t r_i = static_cast<uint8_t>(distr(gen));
        r.push_back(r_i);
        std::cout << static_cast<int>(r_i) << ' '; // generate and print 16 random uint8_t numbers
    }
    std::cout << std::endl;

    std::cout << "r shares: ";
    for (int n=0; n<16; ++n) {
        auto r_i_shares = share_gf256(r[n], 3, 1);
        std::cout << "{ " << static_cast<int>(r_i_shares[0].second) << ", " << static_cast<int>(r_i_shares[1].second) << ", " << static_cast<int>(r_i_shares[2].second) << " }" << std::endl;
    }
    std::cout << std::endl;
}

uint32_t convertToUint32(const std::vector<uint8_t>& array) {
    uint32_t result = 0;
    result |= array[0];                // Least significant byte
    result |= uint32_t(array[1]) << 8;
    result |= uint32_t(array[2]) << 16;
    result |= uint32_t(array[3]) << 24; // Most significant byte
    return result;
}

std::vector<uint8_t> convertToUint8Vector(uint32_t value, size_t total_bytes) {
    std::vector<uint8_t> array;

    // Extract each byte from the uint32_t and insert into the vector
    array.push_back(static_cast<uint8_t>(value & 0xFF));               // Least significant byte
    array.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    array.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    array.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));       // Most significant byte

    // Add padding with zeroes to reach the desired size
    while (array.size() < total_bytes) {
        array.push_back(0);
    }

    return array;
}

void printVector(std::vector<uint8_t> vec) {
    for (int i = 0; i < vec.size(); i++) {
        std::cout << static_cast<int>(vec[i]) << ", ";
    }
    std::cout << std::endl;
}