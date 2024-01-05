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
bool verify_polynomial(const std::vector<uint32_t>& input, int64_t p);

// GF256 (binary) shares
// Note: the API is uint8, but in practice everything operates over ints. This can be made more efficient but probably not a bottle neck.
void generate_tables();
std::vector<std::pair<uint8_t, uint8_t>> share_gf256(uint8_t secret, int n, int t);
uint8_t reconstruct_gf256(const std::vector<std::pair<uint8_t, uint8_t>>& shares);
std::vector<std::vector<std::pair<uint8_t, uint8_t>>> share_gf256_vector(const std::vector<uint8_t>& secrets, int n, int t);
std::vector<uint8_t> reconstruct_gf256_vector(const std::vector<std::vector<std::pair<uint8_t, uint8_t>>>& all_shares);
std::vector<std::pair<uint8_t, uint8_t>> xor_shares_vector(const std::vector<std::pair<uint8_t, uint8_t>>& x_shares,
                                                           const std::vector<std::pair<uint8_t, uint8_t>>& y_shares);
std::pair<uint8_t, uint8_t> xor_shares(const std::pair<uint8_t, uint8_t>& x_shares,
                                       const std::pair<uint8_t, uint8_t>& y_shares);
std::vector<uint8_t> extract_values_gf256(std::vector<std::pair<uint8_t, uint8_t>> shares);
std::vector<std::pair<uint8_t, uint8_t>> encode_to_shares_gf256(const std::vector<uint8_t>& input);

#endif //DPFPIR_SHAMIR_H
