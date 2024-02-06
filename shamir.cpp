//
// Created by Guy Zyskind on 08/12/2022.
//

#include "shamir.h"

#include <iostream>
#include <vector>
#include <cmath>
#include "utils.h"
#include <random>
#include <stdexcept>
#include <cassert>

// Compute the dot product of two vectors over Zp
int64_t dot_product(std::vector<int64_t> a, std::vector<int64_t> b, int64_t p) {
    if (a.size() != b.size()) {
        std::cout << "Error: vectors are not the same size" << std::endl;
        return 0;
    }

    int64_t result = 0;
    for (int64_t i = 0; i < a.size(); i++) {
        result += (a[i] * b[i]) % p;
        result = result % p;
    }

    return result;
}

// Evaluate a polynomial at a given point64_t over Zp
int64_t poly_eval(std::vector<int64_t> coeffs, int64_t x, int64_t p) {
    int64_t result = 0;
    for (int64_t i = 0; i < coeffs.size(); i++) {
        int64_t term = coeffs[i] * static_cast<int64_t>(std::pow(x, i)) % p;
        term = term % p;
        result += term;
        result = result % p;
    }

    return result;
}

// Generate a random polynomial with a given degree and secret value over Zp
std::vector<int64_t> gen_random_poly(int64_t degree, int64_t secret, int64_t p) {
    std::vector<int64_t> coeffs;
    coeffs.push_back(secret);
    for (int64_t i = 1; i < degree; i++) {
        coeffs.push_back(rand() % p);
    }

    return coeffs;
}

// Generate a set of shares for a given secret and number of shares over Zp
std::vector<std::pair<int64_t, int64_t>> gen_shares(int64_t num_shares, int64_t degree, int64_t secret, int64_t p) {
//    int64_t degree = num_shares - 1;
    std::vector<int64_t> coeffs = gen_random_poly(degree, secret, p);

    std::vector<std::pair<int64_t, int64_t>> shares;
    for (int64_t i = 1; i <= num_shares; i++) {
        shares.push_back(std::make_pair(i, poly_eval(coeffs, i, p)));
    }

    return shares;
}

int64_t modexp(int64_t a, int64_t b, int64_t p) {
    int64_t res = 1;
    while (b > 0) {
        if (b & 1) res = (res * a) % p;
        a = (a * a) % p;
        b >>= 1;
    }
    return res;
}

// define the Lagrange interpolation function
int64_t lagrange_interp(std::vector<int64_t> x, std::vector<int64_t> y, int64_t x0, int64_t p) {
    int64_t n = x.size();

    // initialize the result to 0
    int64_t res = 0;

    // compute the interpolation polynomial
    for (int64_t i = 0; i < n; i++) {
        int64_t term = y[i];
        for (int64_t j = 0; j < n; j++) {
            if (i == j) continue;
            term = (term * (x0 - x[j])) % p;
            term = (term * modexp((x[i] - x[j]), p-2, p)) % p;
        }
        res = (res + term) % p;
    }

    // return the result
    return res;
}

// Helper function to encode a vector of uint32_t into shares format
std::vector<std::pair<int64_t, int64_t>> encode_to_shares(const std::vector<uint32_t>& input) {
    std::vector<std::pair<int64_t, int64_t>> shares;
    for (size_t idx = 0; idx < input.size(); ++idx) {
        shares.emplace_back(static_cast<int64_t>(idx + 1), static_cast<int64_t>(input[idx]));
    }
    return shares;
}

std::vector<std::pair<uint8_t, uint8_t>> encode_to_shares_gf256(const std::vector<uint8_t>& input) {
    std::vector<std::pair<uint8_t, uint8_t>> shares;
    for (size_t idx = 0; idx < input.size(); ++idx) {
        shares.emplace_back(static_cast<uint8_t>(idx + 1), static_cast<uint8_t>(input[idx]));
    }
    return shares;
}

// Helper function to check if all three shares fall on the same line
// (i.e., this is a valid degree-1 polynomial)
bool verify_polynomial(const std::vector<uint32_t>& input, int64_t p) {
    int64_t val = lagrange_interp({1, 2}, {input[0], input[1]}, 3, p);

    std::cout << "Original share: " << input[2] << ", reinterpolated share: " << val << std::endl;
    return mod(input[2], p) == mod(val, p);
}


// Recover the secret value from a given set of shares over Zp
int64_t recover_secret(std::vector<std::pair<int64_t, int64_t>> shares, int64_t p) {
    int64_t num_shares = shares.size();
    int64_t degree = num_shares - 1;
    std::vector<int64_t> x_values, y_values;

    for (int64_t i = 0; i < num_shares; i++) {
        x_values.push_back(shares[i].first);
        y_values.push_back(shares[i].second);
    }
    auto res = lagrange_interp(x_values, y_values, 0, p);
    return mod(res,p);
//    assert(mod(res,p) == res); // TODO: remove temp
//    return lagrange_interp(x_values, y_values, 0, p);
}




// Global Definitions
const int FIELD_SIZE = 256;
std::vector<int> exp_table(FIELD_SIZE * 2);
std::vector<int> log_table(FIELD_SIZE);

void generate_tables() {
    int x = 1;
    for (int i = 0; i < FIELD_SIZE - 1; i++) {
        exp_table[i] = x;
        log_table[x] = i;
        x = (x * 2) ^ ((x >= 128) ? 0x11d : 0);
    }
    for (int i = FIELD_SIZE - 1; i < FIELD_SIZE * 2 - 1; i++) {
        exp_table[i] = exp_table[i - FIELD_SIZE + 1];
    }
}

int gf256_add(int a, int b) {
    return a ^ b;
}

int gf256_mult(int a, int b) {
    if (a == 0 || b == 0) return 0;
    return exp_table[(log_table[a] + log_table[b]) % (FIELD_SIZE - 1)];
}

int gf256_inv(int a) {
    if (a == 0) throw std::invalid_argument("Inverse of 0 does not exist.");
    return exp_table[FIELD_SIZE - 1 - log_table[a]];
}


int gf256_eval_poly(const std::vector<int>& poly, uint8_t x) {
    int y = 0;
    for (int i = poly.size() - 1; i >= 0; i--) {
        y = gf256_add(gf256_mult(y, x), poly[i]);
    }
    return y;
}

std::vector<std::pair<uint8_t, uint8_t>> share_gf256(uint8_t secret, int n, int t) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint8_t> distrib(1, FIELD_SIZE - 1);
    int k = t + 1;

    std::vector<int> coefficients(k);
    coefficients[0] = secret;
    for (int i = 1; i < k; ++i) {
        coefficients[i] = distrib(gen);
    }

    std::vector<std::pair<uint8_t, uint8_t>> shares;
    for (int i = 1; i <= n; ++i) {
        shares.push_back({static_cast<uint8_t>(i), static_cast<uint8_t>(gf256_eval_poly(coefficients, i))});
    }

    return shares;
}

uint8_t gf256_interpolate(const std::vector<uint8_t>& x, const std::vector<uint8_t>& y, uint8_t at) {
    int result = 0;
    for (size_t i = 0; i < x.size(); i++) {
        int term = y[i];
        for (size_t j = 0; j < x.size(); j++) {
            if (i != j) {
                int base = gf256_add(at, x[j]);
                int denom = gf256_add(x[i], x[j]);
                term = gf256_mult(term, gf256_mult(base, gf256_inv(denom)));
            }
        }
        result = gf256_add(result, term);
    }
    return static_cast<uint8_t>(result);
}

uint8_t reconstruct_gf256(const std::vector<std::pair<uint8_t, uint8_t>>& shares) {
    std::vector<uint8_t> x_values;
    std::vector<uint8_t> y_values;

    for (const auto& share : shares) {
        x_values.push_back(share.first);
        y_values.push_back(share.second);
    }

    return gf256_interpolate(x_values, y_values, 0);
}

std::vector<std::vector<std::pair<uint8_t, uint8_t>>> share_gf256_vector(const std::vector<uint8_t>& secrets, int n, int t) {
    std::vector<std::vector<std::pair<uint8_t, uint8_t>>> all_shares;
    for (uint8_t secret : secrets) {
        all_shares.push_back(share_gf256(secret, n, t));
    }
    return all_shares;
}

std::vector<uint8_t> reconstruct_gf256_vector(const std::vector<std::vector<std::pair<uint8_t, uint8_t>>>& all_shares) {
    std::vector<uint8_t> reconstructed_secrets;
    for (const auto& shares : all_shares) {
        reconstructed_secrets.push_back(reconstruct_gf256(shares));
    }
    return reconstructed_secrets;
}

std::vector<std::pair<uint8_t, uint8_t>> xor_shares_vector(const std::vector<std::pair<uint8_t, uint8_t>>& x_shares,
                                                           const std::vector<std::pair<uint8_t, uint8_t>>& y_shares) {
    if (x_shares.size() != y_shares.size()) {
        throw std::invalid_argument("Vectors of shares must be of the same size.");
    }

    std::vector<std::pair<uint8_t, uint8_t>> z_shares;
    for (size_t i = 0; i < x_shares.size(); ++i) {
        if (x_shares[i].first != y_shares[i].first) {
            throw std::invalid_argument("Share indices do not match.");
        }
        z_shares.push_back({x_shares[i].first, static_cast<uint8_t>(gf256_add(x_shares[i].second, y_shares[i].second))});
    }

    return z_shares;
}

std::pair<uint8_t, uint8_t> xor_shares(const std::pair<uint8_t, uint8_t>& x_shares,
                                       const std::pair<uint8_t, uint8_t>& y_shares) {
    if (x_shares.first != y_shares.first) {
        throw std::invalid_argument("Share indices do not match.");
    }

    return std::make_pair(x_shares.first, static_cast<uint8_t>(gf256_add(x_shares.second, y_shares.second)));
}

std::vector<uint8_t> extract_values_gf256(std::vector<std::pair<uint8_t, uint8_t>> shares) {
    std::vector<uint8_t> res;
    for (int i = 0; i < shares.size(); i++) {
        res.push_back(shares[i].second);
    }

    return res;
}

uint64_t reverseBits(uint64_t n)
{
    uint64_t rev = 0;

    // traversing bits of 'n' from the right
    while (n > 0) {
        // bitwise left shift
        // 'rev' by 1
        rev <<= 1;

        // if current bit is '1'
        if (n & 1 == 1)
            rev ^= 1;

        // bitwise right shift
        // 'n' by 1
        n >>= 1;
    }

    // required number
    return rev;
}

std::vector<uint64_t> additive_share(uint64_t secret, int num_shares, bool is_xor) {
    std::vector<uint64_t> shares;
    shares.push_back(0);
    uint64_t r = 0;
    for (int i = 0; i < num_shares - 1; i++) {
        uint64_t rr = rand(); // NOTE: in practice it's a 32-bit insecure integer, but okay for testing
        shares.push_back(rr);
        if (is_xor) {
            r ^= rr;
        } else {
            r += rr;
        }
    }
    if (is_xor) {
//        std::cout<< "reverse: " << reverseBits(secret) << std::endl;
//        shares[0] = reverseBits(secret) ^ r;
        shares[0] = secret ^ r;
    } else {
        shares[0] = secret - r;
    }

    return shares;
}

uint64_t additive_reconstruct(std::vector<uint64_t>& shares, bool is_xor) {
    uint64_t res = 0;
    for (int i = 0; i < shares.size(); i++) {
        if (is_xor) {
            res ^= shares[i];
        } else {
            res += shares[i];
        }
    }

    return res;
}

std::vector<block> additive_share(block secret, int num_shares) {
    std::vector<block> shares;
    shares.push_back(_mm_setzero_si128());
    block r = _mm_setzero_si128();
    for (int i = 0; i < num_shares - 1; i++) {
        block rr = generate_random_128bit_number();
        shares.push_back(rr);
        r = _mm_xor_si128(r, rr);
    }
    shares[0] = _mm_xor_si128(secret, r);

    return shares;
}

block additive_reconstruct(std::vector<block>& shares) {
    block res = _mm_setzero_si128();
    for (int i = 0; i < shares.size(); i++) {
        res = _mm_xor_si128(res, shares[i]);
    }

    return res;
}

