//
// Created by Guy Zyskind on 08/12/2022.
//

#include "shamir.h"

#include <iostream>
#include <vector>
#include <cmath>
#include "utils.h"

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