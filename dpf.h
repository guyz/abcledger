#pragma once

#include <cstdlib>
#include <vector>

#include "Defines.h"
namespace DPF {
    std::pair<std::vector<uint8_t>, std::vector<uint8_t> > Gen(size_t alpha, size_t logn);
    bool Eval(const std::vector<uint8_t>& key, size_t x, size_t logn);
    std::vector<uint8_t> EvalFull(const std::vector<uint8_t>& key, size_t logn);
    std::vector<uint8_t> EvalFull8(const std::vector<uint8_t>& key, size_t logn);

    std::pair<std::array<std::vector<uint8_t>, 31>, std::array<std::vector<uint8_t>, 31>> GenM1bit(size_t alpha, size_t logn, int32_t msg);
    std::vector<int32_t> EvalFullM1bit(const std::array<std::vector<uint8_t>, 31>& key, size_t logn);

    // 32-bit messages
    std::pair<std::vector<uint8_t>, std::vector<uint8_t>> GenM(size_t alpha, size_t logn, uint32_t msg);
    void EvalFullRecursive8M(const std::vector<uint8_t>& key, std::array<block, 8>& s, std::array<uint8_t,8>& t, size_t lvl, size_t stop, std::array<uint8_t*,8>& res, block *CW, bool party_index = false);
    std::vector<uint32_t> EvalFull8M(const std::vector<uint8_t>& key, size_t logn, bool party_index = false);

    // New DPF constructions (Updated 2023)
    // (1,2)-DPF+
    std::pair<std::pair<uint32_t, std::vector<uint8_t>>, std::pair<uint32_t, std::vector<uint8_t>>>
    GenP(size_t alpha, size_t logn, uint32_t m1, uint32_t m2);
    std::vector<uint32_t> EvalFull8P(const std::pair<uint32_t, std::vector<uint8_t>>& key, size_t logn, bool party_index = false);





}
