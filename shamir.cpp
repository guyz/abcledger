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

    return lagrange_interp(x_values, y_values, 0, p);
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

int gf256_eval_poly(const std::vector<int>& poly, int x) {
    int y = 0;
    for (int i = poly.size() - 1; i >= 0; i--) {
        y = gf256_add(gf256_mult(y, x), poly[i]);
    }
    return y;
}

std::vector<std::pair<int, int>> share_gf256(int secret, int n, int t) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(1, FIELD_SIZE - 1);
    int k = t + 1;

    std::vector<int> coefficients(k);
    coefficients[0] = secret;
    for (int i = 1; i < k; ++i) {
        coefficients[i] = distrib(gen);
    }

    std::vector<std::pair<int, int>> shares;
    for (int i = 1; i <= n; ++i) {
        shares.push_back({i, gf256_eval_poly(coefficients, i)});
    }

    return shares;
}

int gf256_interpolate(const std::vector<int>& x, const std::vector<int>& y, int at) {
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
    return result;
}

int reconstruct_gf256(const std::vector<std::pair<int, int>>& shares) {
    std::vector<int> x_values;
    std::vector<int> y_values;

    for (const auto& share : shares) {
        x_values.push_back(share.first);
        y_values.push_back(share.second);
    }

    return gf256_interpolate(x_values, y_values, 0);
}

std::vector<std::vector<std::pair<int, int>>> share_gf256_vector(const std::vector<int>& secrets, int n, int t) {
    std::vector<std::vector<std::pair<int, int>>> all_shares;
    for (int secret : secrets) {
        all_shares.push_back(share_gf256(secret, n, t));
    }
    return all_shares;
}

std::vector<int> reconstruct_gf256_vector(const std::vector<std::vector<std::pair<int, int>>>& all_shares) {
    std::vector<int> reconstructed_secrets;
    for (const auto& shares : all_shares) {
        reconstructed_secrets.push_back(reconstruct_gf256(shares));
    }
    return reconstructed_secrets;
}

// Function to XOR shares of two vectors
std::vector<std::pair<int, int>> xor_shares_vector(const std::vector<std::pair<int, int>>& x_shares,
                                                   const std::vector<std::pair<int, int>>& y_shares) {
    if (x_shares.size() != y_shares.size()) {
        throw std::invalid_argument("Vectors of shares must be of the same size.");
    }

    std::vector<std::pair<int, int>> z_shares;
    for (size_t i = 0; i < x_shares.size(); ++i) {
        if (x_shares[i].first != y_shares[i].first) {
            throw std::invalid_argument("Share indices do not match.");
        }
        z_shares.push_back({x_shares[i].first, gf256_add(x_shares[i].second, y_shares[i].second)});
    }

    return z_shares;
}