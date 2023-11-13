//
// Created by Guy Zyskind on 08/12/2022.
//

#ifndef DPFPIR_SHAMIR_H
#define DPFPIR_SHAMIR_H

#include <iostream>
#include <vector>
#include <cmath>

int64_t dot_product(std::vector<int64_t> a, std::vector<int64_t> b, int64_t p);
int64_t poly_eval(std::vector<int64_t> coeffs, int64_t x, int64_t p);
std::vector<int64_t> gen_random_poly(int64_t degree, int64_t secret, int64_t p);
std::vector<std::pair<int64_t, int64_t>> gen_shares(int64_t num_shares, int64_t degree, int64_t secret, int64_t p);
int64_t recover_secret(std::vector<std::pair<int64_t, int64_t>> shares, int64_t p);
std::vector<std::pair<int64_t, int64_t>> encode_to_shares(const std::vector<uint32_t>& input);

#endif //DPFPIR_SHAMIR_H
