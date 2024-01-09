#include "dpf.h"
#include "pirw.h"
#include "Server.h"
#include <chrono>
#include <iostream>
#include <cassert>
#include <immintrin.h>
#include "shamir.h"
#include <cstring>
#include "utils.h"
#include <cstdlib>
#include <random>
#include <fstream>
#include <vector>
#include <cstdint>
#include <filesystem>
#include <string>
#include "utils.h"

inline __m256i mul256(__m256i x1, __m256i x2) {
    return _mm256_mullo_epi64(x1, x2);
}

void benchmark_mersenne() {
    const size_t N = 10000000; // Number of elements
    const int REPEATS = 10;    // Number of repeats

    // Random number generation
    std::mt19937 rng;
    rng.seed(std::random_device()());
    std::uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);

    // Create a vector of random uint32_t values
    std::vector<uint32_t> values(N);
    for (size_t i = 0; i < N; ++i) {
        values[i] = dist(rng);
    }

    // Benchmark modmersenne31
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < REPEATS; ++i) {
        for (size_t j = 0; j < N; ++j) {
            modmersenne31(values[j]);
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed1 = end - start;
    double avgTime1 = elapsed1.count() / REPEATS;

    // Benchmark modmersenne31safe64
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < REPEATS; ++i) {
        for (size_t j = 0; j < N; ++j) {
            modmersenne31safe64(values[j]);
        }
    }
    end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed2 = end - start;
    double avgTime2 = elapsed2.count() / REPEATS;

    // Print results
    std::cout << "Average execution time for modmersenne31: " << avgTime1 << " seconds." << std::endl;
    std::cout << "Average execution time for modmersenne31safe64: " << avgTime2 << " seconds." << std::endl;

    // Benchmark (values[j] % PP)
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < REPEATS; ++i) {
        for (size_t j = 0; j < N; ++j) {
            volatile uint32_t result = values[j] % PP;  // volatile to prevent optimization
        }
    }
    end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed3 = end - start;
    double avgTime3 = elapsed3.count() / REPEATS;

    // Print result for the modulus operation - testing with none mersenne prime to see if it matters.
    std::cout << "Average execution time for (values[j] % PP): " << avgTime3 << " seconds." << std::endl;

    // Benchmark (values[j] % PP)
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < REPEATS; ++i) {
        for (size_t j = 0; j < N; ++j) {
            volatile uint32_t result = values[j] % OTHER_PRIME;  // volatile to prevent optimization
        }
    }
    end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed4 = end - start;
    double avgTime4 = elapsed3.count() / REPEATS;

    // Print result for the modulus operation
    std::cout << "Average execution time for (values[j] % OTHER_PRIME): " << avgTime4 << " seconds." << std::endl;

}

void test_sumproduct() {
    const uint64_t N = 10000;
    std::vector<uint32_t> x(N), y(N), x1(N), x2(N), x3(N), y1(N), y2(N), y3(N);

    // Fill the array with some values
    for (int i = 0; i < N; i++) {
        x[i] = rand() % (1 << 15) - 1;
        y[i] = rand() % (1 << 15) - 1;

        std::vector<std::pair<int64_t, int64_t>> x_i_shares = gen_shares(3, 2, x[i], PP);
        std::vector<std::pair<int64_t, int64_t>> y_i_shares = gen_shares(3, 2, y[i], PP);

        x1[i] = x_i_shares[0].second;
        x2[i] = x_i_shares[1].second;
        x3[i] = x_i_shares[2].second;

        y1[i] = y_i_shares[0].second;
        y2[i] = y_i_shares[1].second;
        y3[i] = y_i_shares[2].second;
    }

    auto product_share1 = PIRW::innerprodff31(x1, y1);
    auto product_share2 = PIRW::innerprodff31(x2, y2);
    auto product_share3 = PIRW::innerprodff31(x3, y3);
    auto product = PIRW::innerprodff31(x, y);

    auto shares = encode_to_shares({
                                           product_share1,
                                           product_share2,
                                           product_share3
                                   });
    auto xy = recover_secret(shares, PP);
    assert(xy == product);
    std::cout << "Inner product works! " << std::endl;
}

int run_playground_tests(int N) {
    std::chrono::duration<double> buildT, evalT1, evalT2, evalT3;
    size_t keysizeT = 0;
    buildT = evalT1 = evalT2 = evalT3 = std::chrono::duration<double>::zero();

    // Benchmark a single execution of Gen
    auto time1 = std::chrono::high_resolution_clock::now();
    auto keys = DPF::Gen(0, N);
    auto a = keys.first;
    auto b = keys.second;
    keysizeT = a.size();
    auto time2 = std::chrono::high_resolution_clock::now();

    // Benchmark a single execution of EvalFull
    std::vector<uint8_t> v0, v1;
    v0 = DPF::EvalFull(a, N);
    auto time3 = std::chrono::high_resolution_clock::now();

    if(N > 10) {
        v1 = DPF::EvalFull8(a, N);
    }
    auto time4 = std::chrono::high_resolution_clock::now();

    buildT += time2 - time1;
    evalT1 += time3 - time2;
    evalT2 += time4 - time3;

    std::cout << "DPF.Gen: "        <<  buildT.count() << "sec" << std::endl;
    std::cout << "DPF.Eval: "       << evalT1.count() << "sec" << std::endl;
    std::cout << "DPF.Eval8 "     << evalT2.count() << "sec" << std::endl;
    std::cout << keysizeT << "; "   <<  N*32 << " bytes total transfer" << std::endl;

    // Benchmark N length 31-bit DPF vs (N + 5) 1-bit DPF vs length N 32-bit DPF. Without threading, ideally, should take about the same time
    size_t N1 = N + 5;

    keys = DPF::Gen(0, N1);
    a = keys.first;
    b = keys.second;

    auto keys2 = DPF::GenM1bit(0, N, 25);
    auto a2 = keys2.first;
    auto b2 = keys2.second;

    auto keys3 = DPF::GenM(0, N, 25);
    auto a3 = keys.first;
    auto b3 = keys.second;

    time1 = std::chrono::high_resolution_clock::now();
    v0 = DPF::EvalFull8(a, N1);
    time2 = std::chrono::high_resolution_clock::now();
    auto u = DPF::EvalFullM1bit(a2, N);
    time3 = std::chrono::high_resolution_clock::now();
    auto w = DPF::EvalFull8M(a3, N, 1);
    time4 = std::chrono::high_resolution_clock::now();

    evalT1 = time2 - time1;
    evalT2 = time3 - time2;
    evalT3 = time4 - time3;
    std::cout << "DPF.EvalFull8: "       << evalT1.count() << "sec" << std::endl;
    std::cout << "DPF.EvalFullM1bit "     << evalT2.count() << "sec" << std::endl;
    std::cout << "DPF.EvalFull8M "     << evalT3.count() << "sec" << std::endl;

    // test correctness
    if (false){
        return 0;
    }

//    uint32_t arr[4] = {3995864550, 725568960, 3911890762, 3379270938};
    uint32_t arr[4] = {15, 34, 42, 57};
    block blk = _mm_set_epi32(arr[3], arr[2], arr[1], arr[0]);

    reg_arr_union tmp = {ZeroBlock};
    tmp.reg = modmersenne31block(blk);

    uint32_t arr2[4] = {3995864550, 725568960, 3911890762, 3379270938};
    blk = _mm_set_epi32(arr2[3], arr2[2], arr2[1], arr2[0]);
    tmp = {ZeroBlock};
    tmp.reg = modmersenne31block(blk);

    blk = _mm_set_epi32(arr2[0], arr2[1], arr2[2], arr2[3]);
    tmp = {ZeroBlock};
    tmp.reg = modmersenne31block(blk);
    srand(time(0));
    for (int j; j < 100; j++) {
        N = 10;
        int alpha = rand() % (1 << N) - 1; // 22;
        // Generate a random number between 1 and 2^30
        int beta = rand() % (1 << 16) + 1; // 25;
        auto km = DPF::GenM(alpha, N, beta);
        auto km0 = km.first;
        auto km1 = km.second;
        auto vm0 = DPF::EvalFull8M(km0, N);
        auto vm1 = DPF::EvalFull8M(km1, N, true);

        // DPF+ tests
        int beta1 = rand() % (1 << 16) + 1; // 25;
        int beta2 = beta ^ beta1;
        auto kmp = DPF::GenP(alpha, N, beta1, beta2);
        auto kmp0 = kmp.first;
        auto kmp1 = kmp.second;
        auto vmp0 = DPF::EvalFull8P(kmp0, N).first;
        auto vmp1 = DPF::EvalFull8P(kmp1, N, true).first;

        auto kms = DPF::GenShamir(alpha, N, beta);
        auto kms0 = kms[0];
        auto kms1 = kms[1];
        auto kms2 = kms[2];
        auto vms0 = DPF::EvalShamir(kms0, N, 0).first;
        auto vms1 = DPF::EvalShamir(kms1, N, 1).first;
        auto vms2 = DPF::EvalShamir(kms2, N, 2).first;

        // VerDPF tests
        auto kmv = DPF::VerGenM(alpha, N, beta);
        auto vmv0 = DPF::VerEvalFull8M(kmv.first, N, false);
        auto vmv1 = DPF::VerEvalFull8M(kmv.second, N, true);

        auto h1 = vmv0.second;
        auto h2 = vmv1.second;

        std::cout << "h1: ";
        for (const auto& e : h1) {
            std::cout << "[";
            for (int i = 0; i < sizeof(block) / sizeof(uint32_t); ++i) {
                std::cout << std::hex << ((uint32_t*)&e)[i] << (i < sizeof(block) / sizeof(uint32_t) - 1 ? " " : "");
            }
            std::cout << "] ";
        }
        std::cout << std::endl;

        std::cout << "h2: ";
        for (const auto& e : h2) {
            std::cout << "[";
            for (int i = 0; i < sizeof(block) / sizeof(uint32_t); ++i) {
                std::cout << std::hex << ((uint32_t*)&e)[i] << (i < sizeof(block) / sizeof(uint32_t) - 1 ? " " : "");
            }
            std::cout << "] ";
        }
        std::cout << std::endl;
        assert(are_arrays_equal(h1, h2));

        for (int i = 0; i < vm0.size(); i++) {
            ////        std::cout << "a1[" << i << "] =" << a1[i] << std::endl;
            ////        std::cout << "a2[" << i << "] =" << a2[i] << std::endl;
            //        std::cout << "res[" << i << "] = " << modmersenne31(vm0[i] + vm1[i]) << std::endl;
            if (i == alpha) {
                std::cout << "res[" << i << "] = " << (vm0[i] ^ vm1[i]) << " " << vm0[i] << " " << vm1[i]
                          << " " << beta << std::endl;
                assert( (vm0[i] ^ vm1[i]) == beta);
                assert( (vmp0[i] ^ vmp1[i]) == beta);
                std::cout << "vm[" << i << "]: Share1: " << vms0[i] << ", Share2: " << vms1[i] << ", Share3: " << vms2[i] << std::endl;
                assert(vmp0[i] == beta1);
                assert(vmp1[i] == beta2);
                auto shares = encode_to_shares({vms0[i], vms1[i], vms2[i]});
                auto vv = recover_secret(shares, PP);
                assert(vv == beta);
            } else {
                std::cout << "vm[" << i << "]: Share1: " << vms0[i] << ", Share2: " << vms1[i] << ", Share3: " << vms2[i] << std::endl;
                assert( (vm0[i] ^ vm1[i]) == 0);
                assert( (vmp0[i] ^ vmp1[i]) == 0);

                auto shares = encode_to_shares({vms0[i], vms1[i], vms2[i]});
                auto vv = recover_secret(shares, PP);
                assert(vv == 0);
//                assert( (vms0[i] ^ vms1[i]) == 0);
//                assert(modmersenne31(vm0[i] + vm1[i]) == 0);
//                assert(modmersenne31(vmp0[i] + vmp1[i]) == 0);
            }

        }

//        for (int i = 0; i < vm0.size(); i++) {
//            ////        std::cout << "a1[" << i << "] =" << a1[i] << std::endl;
//            ////        std::cout << "a2[" << i << "] =" << a2[i] << std::endl;
//            //        std::cout << "res[" << i << "] = " << modmersenne31(vm0[i] + vm1[i]) << std::endl;
//            if (i == alpha) {
//                std::cout << "res[" << i << "] = " << modmersenne31(vm0[i] + vm1[i]) << " " << vm0[i] << " " << vm1[i]
//                          << " " << beta << std::endl;
//                assert(modmersenne31(   vm0[i] + vm1[i]) == beta);
//                assert(modmersenne31(   vmp0[i] + vmp1[i]) == beta);
//                assert(vmp0[i] == beta1);
//                assert(vmp1[i] == beta2);
//            } else {
//                assert(modmersenne31(vm0[i] + vm1[i]) == 0);
////                assert(modmersenne31(vmp0[i] + vmp1[i]) == 0);
//            }
//
//        }
    }

    std::cout << "GenM and EvalFull8M works!" << std::endl;

    // Try packing
//    _mm256_cvtepi32_epi64
//    uint32_t arr[4] = {23423412, 467456111, 123123112, 546756113};
//    __m128i x = _mm_set_epi32(arr[3], arr[2], arr[1], arr[0]);
//    __m256i x1 = _mm256_cvtepi32_epi64(x);
//    long long* ptr = (long long*)&x1;
//    printf("%lld %lld %lld %lld\n", ptr[0], ptr[1], ptr[2], ptr[3]);
//
//    // Try mult 128-128 - THIS DOES NOT WORK!
//    __m128i y = _mm_mul_epi32(x, x);
//
//    int* ptr2 = (int*)&y;
//    printf("%d %d %d %d\n", ptr2[0], ptr2[1], ptr2[2], ptr2[3]);
//
//    // Try mult 256-256 - THIS WORKS!
//    __m256i y1 = mul256(x1, x1);
//    ptr = (long long*)&y1;
//    printf("%lld %lld %lld %lld\n", ptr[0], ptr[1], ptr[2], ptr[3]);

    // benchmark cast..

    // Define an array of 1000000 int32_t values
//    const uint64_t NNN = 100000000;
    const uint64_t NNN = 16777216;
//    const uint64_t NNN = 268435456;
//    const uint64_t NNN = 1073741824;
    std::vector<uint32_t> x(NNN), y(NNN);

    // Fill the array with some values
    for (int i = 0; i < NNN; i++) {
        x[i] = i;
        y[i] = i;
    }

    // Record the starting time
    auto start = std::chrono::high_resolution_clock::now();

//    auto z = PIRW::addvff31(x, y);
    uint32_t inner_product;
    for (int j = 0; j < 20; j++) {
        inner_product = PIRW::innerprodff31(x, y);
    }

    // Record the ending time
    auto end = std::chrono::high_resolution_clock::now();

    // Compute the elapsed time
    std::chrono::duration<double> elapsed = end - start;

    // Print the elapsed time
    std::cout << "Elapsed time (for inner product of " << NNN << " values): " << elapsed.count()/20.0 << " seconds" << std::endl;

    // Print the elements of c
//    for (const auto& element : z)
//    {
//        std::cout << element << " ";
//    }
//    std::cout << std::endl;

    // Print the inner product
    std::cout << inner_product << std::endl;


    // Try Shamir Sharing
    int s = -5345348;
    int p = 2147483647;
//    int p = 8191;

    std::vector<std::pair<int64_t, int64_t>> shares = gen_shares(3, 2, s, p);
    int s1 = recover_secret(shares, p);
    std::cout << s1 << " is the reconstructed secret " << std::endl;

}

int test_hash_functions() {
    uint32_t r1 = rand();
    uint32_t r2 = rand();
    uint32_t r3 = rand();
    uint32_t r4 = rand();

    // Combine the four 32-bit numbers into a 128-bit number
    block seed = _mm_set_epi32(r1, r2, r3, r4);
    uint32_t alpha = 24323;
    auto res = DPF::prg::hash1(seed, alpha);
    auto res2 = DPF::prg::hash2(res);
    auto res3 = DPF::prg::hash1(seed, alpha);
    auto res4 = DPF::prg::hash2(res);

    // Print the seed
    std::cout << "Seed: " << std::hex << r1 << " " << r2 << " " << r3 << " " << r4 << std::endl;

    // Print alpha
    std::cout << "Alpha: " << alpha << std::endl;

    // Assert that res is equal to res3
    assert(are_arrays_equal(res, res3));

    // Assert that res2 is equal to res4
    assert(are_arrays_equal_2(res2, res4));

    // Print res
    std::cout << "Res: ";
    for (const auto& e : res) {
        std::cout << "[";
        for (int i = 0; i < sizeof(block) / sizeof(uint32_t); ++i) {
            std::cout << std::hex << ((uint32_t*)&e)[i] << (i < sizeof(block) / sizeof(uint32_t) - 1 ? " " : "");
        }
        std::cout << "] ";
    }
    std::cout << std::endl;

    // Print res2
    std::cout << "Res2: ";
    for (const auto& e : res2) {
        std::cout << "[";
        for (int i = 0; i < sizeof(block) / sizeof(uint32_t); ++i) {
            std::cout << std::hex << ((uint32_t*)&e)[i] << (i < sizeof(block) / sizeof(uint32_t) - 1 ? " " : "");
        }
        std::cout << "] ";
    }
    std::cout << std::endl;

    return 0;
}

// TODO: refactor - move these to DPF...
// Serialize
void serialize(const std::vector<DPF::KeyShare>& data, const std::string& filename) {
    std::ofstream out(filename, std::ios::binary);
    for (const auto& keyShare : data) {
        // Serialize 'key'
        uint32_t keySize = keyShare.key.size();
        out.write(reinterpret_cast<const char*>(&keySize), sizeof(keySize));
        out.write(reinterpret_cast<const char*>(keyShare.key.data()), keySize);

        // Serialize 'cs'
        out.write(reinterpret_cast<const char*>(&keyShare.cs), sizeof(keyShare.cs));

        // Serialize 'z'
        out.write(reinterpret_cast<const char*>(&keyShare.z), sizeof(keyShare.z));
    }
    out.close();
}


// Unserialize
std::vector<DPF::KeyShare> unserialize(const std::string& filename) {
    std::vector<DPF::KeyShare> data;
    std::ifstream in(filename, std::ios::binary);
    while (!in.eof()) {
        DPF::KeyShare keyShare;

        // Unserialize 'key'
        uint32_t keySize;
        in.read(reinterpret_cast<char*>(&keySize), sizeof(keySize));
        keyShare.key.resize(keySize);
        in.read(reinterpret_cast<char*>(keyShare.key.data()), keySize);

        // Unserialize 'cs'
        in.read(reinterpret_cast<char*>(&keyShare.cs), sizeof(keyShare.cs));

        // Unserialize 'z'
        in.read(reinterpret_cast<char*>(&keyShare.z), sizeof(keyShare.z));

        data.push_back(keyShare);
    }
    in.close();
    return data;
}

void writeVector(int index, const std::vector<DPF::KeyShare>& keyShares, std::string filename) {
    serialize(keyShares, DATA_DIR + filename + std::to_string(index) + ".txt");
}

std::vector<DPF::KeyShare> loadVector(int index, std::string filename) {
    return unserialize(DATA_DIR + filename + std::to_string(index) + ".txt");
}

// Helper function to serialize a vector of pairs
template<typename T1, typename T2>
void serializeVectorPair(const std::vector<std::pair<T1, T2>>& vec, std::ofstream& outFile) {
    size_t vecSize = vec.size();
    outFile.write(reinterpret_cast<const char*>(&vecSize), sizeof(vecSize));
    for (const auto& pair : vec) {
        outFile.write(reinterpret_cast<const char*>(&pair.first), sizeof(pair.first));
        outFile.write(reinterpret_cast<const char*>(&pair.second), sizeof(pair.second));
    }
}

// Helper function to deserialize a vector of pairs
template<typename T1, typename T2>
std::vector<std::pair<T1, T2>> deserializeVectorPair(std::ifstream& inFile) {
    std::vector<std::pair<T1, T2>> vec;
    size_t vecSize;
    inFile.read(reinterpret_cast<char*>(&vecSize), sizeof(vecSize));
    vec.resize(vecSize);
    for (auto& pair : vec) {
        inFile.read(reinterpret_cast<char*>(&pair.first), sizeof(pair.first));
        inFile.read(reinterpret_cast<char*>(&pair.second), sizeof(pair.second));
    }
    return vec;
}

// Helper function to serialize a DeferredKeyShare
void serializeDeferredKeyShare(const DPF::DeferredKeyShare& dks, std::ofstream& outFile) {
    // Serialize the 'key' vector
    size_t keySize = dks.key.size();
    outFile.write(reinterpret_cast<const char*>(&keySize), sizeof(keySize));
    outFile.write(reinterpret_cast<const char*>(dks.key.data()), keySize);

    // Serialize 's0_share' and 's1_share' vectors
    serializeVectorPair(dks.s0_share, outFile);
    serializeVectorPair(dks.s1_share, outFile);

    // Serialize 't0_share'
    outFile.write(reinterpret_cast<const char*>(&dks.t0_share.first), sizeof(dks.t0_share.first));
    outFile.write(reinterpret_cast<const char*>(&dks.t0_share.second), sizeof(dks.t0_share.second));
}

// Helper function to deserialize a DeferredKeyShare
DPF::DeferredKeyShare deserializeDeferredKeyShare(std::ifstream& inFile) {
    DPF::DeferredKeyShare dks;

    // Deserialize the 'key' vector
    size_t keySize;
    inFile.read(reinterpret_cast<char*>(&keySize), sizeof(keySize));
    dks.key.resize(keySize);
    inFile.read(reinterpret_cast<char*>(dks.key.data()), keySize);

    // Deserialize 's0_share' and 's1_share' vectors
    dks.s0_share = deserializeVectorPair<uint8_t, uint8_t>(inFile);
    dks.s1_share = deserializeVectorPair<uint8_t, uint8_t>(inFile);

    // Deserialize 't0_share'
    inFile.read(reinterpret_cast<char*>(&dks.t0_share.first), sizeof(dks.t0_share.first));
    inFile.read(reinterpret_cast<char*>(&dks.t0_share.second), sizeof(dks.t0_share.second));

    return dks;
}

// Serialization function
void writePair(const std::pair<DPF::DeferredKeyShare, DPF::DeferredKeyShare>& pair, const std::string& filename) {
    std::ofstream outFile(filename, std::ios::binary);
    if (!outFile.is_open()) {
        throw std::runtime_error("Unable to open file for writing");
    }

    // Serialize first DeferredKeyShare
    serializeDeferredKeyShare(pair.first, outFile);

    // Serialize second DeferredKeyShare
    serializeDeferredKeyShare(pair.second, outFile);

    outFile.close();
}

// Deserialization function
std::pair<DPF::DeferredKeyShare, DPF::DeferredKeyShare> loadPair(const std::string& filename) {
    std::ifstream inFile(filename, std::ios::binary);
    if (!inFile.is_open()) {
        throw std::runtime_error("Unable to open file for reading");
    }

    // Deserialize first DeferredKeyShare
    DPF::DeferredKeyShare first = deserializeDeferredKeyShare(inFile);

    // Deserialize second DeferredKeyShare
    DPF::DeferredKeyShare second = deserializeDeferredKeyShare(inFile);

    inFile.close();
    return {first, second};
}


bool fileExists(const std::string& fileName) {
    return std::filesystem::exists(fileName);
}


void client_send() {

}

// Function to compare two DeferredKeyShare objects for equality
bool areEqual(const DPF::DeferredKeyShare& dks1, const DPF::DeferredKeyShare& dks2) {
    return (dks1.key == dks2.key) &&
           (dks1.s0_share == dks2.s0_share) &&
           (dks1.s1_share == dks2.s1_share) &&
           (dks1.t0_share == dks2.t0_share);
}

// Test function
void testSerialization() {
    // Create a DeferredKeyShare object
    DPF::DeferredKeyShare originalDks;
    originalDks.key = {1, 2, 3, 4, 5};
    originalDks.s0_share = {{1, 2}, {3, 4}};
    originalDks.s1_share = {{5, 6}, {7, 8}};
    originalDks.t0_share = {9, 10};

    // Serialize it to a file
    std::string filename = "test_dks.bin";
    std::ofstream outFile(filename, std::ios::binary);
    serializeDeferredKeyShare(originalDks, outFile);
    outFile.close();

    // Deserialize it back
    std::ifstream inFile(filename, std::ios::binary);
    DPF::DeferredKeyShare deserializedDks = deserializeDeferredKeyShare(inFile);
    inFile.close();

    // Compare the original and deserialized objects
    assert(areEqual(originalDks, deserializedDks));

    std::cout << "Test passed: Serialized and deserialized DeferredKeyShare objects are identical." << std::endl;
}

void test_client(int serverIndex, int logN) {
    int N = 1 << logN;
    int amount = 7;
    int senderIndex = 0;
    int recvIndex = 20;
    uint64_t alpha = 2112445456;

    // Load/generate data
    std::vector<DPF::KeyShare> kmsA_i;
    std::vector<DPF::KeyShare> kmsB_i;

    //todo: 1DPF..

    auto kmsA = DPF::GenShamir(senderIndex, logN, amount, false);
    auto kmsB = DPF::GenShamir(recvIndex, logN, amount, true);

    if (fileExists(DATA_DIR + "kmsA" + std::to_string(serverIndex) + ".txt")) {
        kmsA_i = loadVector(serverIndex, "kmsA");
        kmsB_i = loadVector(serverIndex, "kmsB");
    } else {
        // File does not exist
        for (size_t i = 0; i < kmsA.size(); ++i) {
            writeVector(i, kmsA[i], "kmsA");
            writeVector(i, kmsB[i], "kmsB");
        }

        kmsA_i = kmsA[serverIndex];
        kmsB_i = kmsA[serverIndex];
    }

    field tag_A = (alpha*amount) % PP;
    std::vector<std::pair<int64_t, int64_t>> tag_A_shares = gen_shares(3, 2, tag_A, PP);
    field tag_A_share = tag_A_shares[serverIndex].second;

    // TODO: remove temp
//    std::vector<std::vector<uint32_t>> ledgers(4);  // Create a vector of 3 vectors
//
//    for (int j = 0; j < 4; j++) {
//        std::ifstream ledgerFile;
//
//        // Choose file based on the value of j
//        if (j < 3) {
//            ledgerFile.open("data/ledger-" + std::to_string(j + 1) + ".txt");
//        } else {  // When j == 3
//            ledgerFile.open("data/ledger.txt");
//        }
//
//        // Load ledger if file exists
//        if (ledgerFile) {
//            uint32_t value;
//            while (ledgerFile >> value) {
//                ledgers[j].push_back(value);
//            }
//            ledgerFile.close();
//        }
//    }
//
//    std::cout << "ledger value: " <<  ledgers[3][senderIndex] << std::endl;
//    std::cout << "ledger1 value: " <<  ledgers[0][senderIndex] << std::endl;
//    std::cout << "ledger2 value: " <<  ledgers[1][senderIndex] << std::endl;
//    std::cout << "ledger3 value: " <<  ledgers[2][senderIndex] << std::endl;
//    auto ledgervalueshares = encode_to_shares({
//                                                ledgers[0][senderIndex],
//                                                ledgers[1][senderIndex],
//                                                ledgers[2][senderIndex]
//                                    });
//    auto ledgervalue = recover_secret(ledgervalueshares, PP);
//    std::cout << "ledger reconstructed: " << ledgervalue << std::endl;
//
//    auto loadedKMSA1 = loadVector(0, "kmsA");
//    auto vmsA1 = DPF::EvalShamir(loadedKMSA1, logN, 0);
//    auto vmsA2 = DPF::EvalShamir(loadVector(1, "kmsA"), logN, 1);
//    auto vmsA3 = DPF::EvalShamir(loadVector(2, "kmsA"), logN, 2);
//
//    auto vmsA1v2 = DPF::EvalShamir(kmsA[0], logN, 0);
//    auto vmsA2v2 = DPF::EvalShamir(kmsA[1], logN, 1);
//    auto vmsA3v2 = DPF::EvalShamir(kmsA[2], logN, 2);
//
//    auto tmpres = encode_to_shares({
//                                           vmsA1[0],
//                                           vmsA2[0],
//                                           vmsA3[0]
//                                   });
//    auto tmpresres = recover_secret(tmpres, PP);
//
//    auto tmpres2 = encode_to_shares({
//                                           vmsA1v2[0],
//                                           vmsA2v2[0],
//                                           vmsA3v2[0]
//                                   });
//    auto tmpresres2 = recover_secret(tmpres2, PP);
//    std::cout << "Recovered the first value: " << tmpresres << std::endl;
//    std::cout << "Recovered the first value2: " << tmpresres2 << std::endl;
//    std::cout << "Ledger sizes: " << ledgers[0].size() << ", " << ledgers[1].size() << ", " << ledgers[2].size() << std::endl;
//    std::cout << "vms sizes: " << vmsA1.size() << ", " << vmsA2.size() << ", " << vmsA3.size() << std::endl;
//    std::cout << "vms (not disk-loaded) sizes: " << vmsA1v2.size() << ", " << vmsA2v2.size() << ", " << vmsA3v2.size() << std::endl;
//    uint32_t balance1 = PIRW::innerprodff31(vmsA1, ledgers[0]); // In practice, this is the full SumProduct protocol but we will defer it to the end..?
//    uint32_t balance2 = PIRW::innerprodff31(vmsA2, ledgers[1]); // In practice, this is the full SumProduct protocol but we will defer it to the end..?
//    uint32_t balance3 = PIRW::innerprodff31(vmsA3, ledgers[2]); // In practice, this is the full SumProduct protocol but we will defer it to the end..?
//
//    auto balanceshares = encode_to_shares({balance1, balance2, balance3});
//    std::cout << "Balances: " << balance1 << ", " << balance2 << ", " << balance3 << std::endl;
//    auto balance = recover_secret(balanceshares, PP);
//    std::cout << "Balance computed: " << balance << std::endl;
//
//    uint32_t dpfval1 = PIRW::sumvecff31(vmsA1);
//    uint32_t dpfval2 = PIRW::sumvecff31(vmsA2);
//    uint32_t dpfval3 = PIRW::sumvecff31(vmsA3);
//
//    auto dpfvalshares = encode_to_shares({dpfval1, dpfval2, dpfval3});
//    auto dapval = recover_secret(dpfvalshares, PP);
//    std::cout << "DPF Value computed: " << dapval << std::endl;
////    auto val = recover_secret(encode_to_shares({(vmsA1[0]*ledgers[0][0] % PP), (vmsA2[0]*ledgers[1][0] % PP), (vmsA3[0]*ledgers[2][0] % PP)}), PP);
//    auto valshares = encode_to_shares({(vmsA1[0]*ledgers[0][0] % PP), (vmsA2[0]*ledgers[1][0] % PP), (vmsA3[0]*ledgers[2][0] % PP)});
//    auto val = recover_secret(valshares, PP);
//    std::cout << "Trying to reconstruct the first multiplicative value: " << val << std::endl;
//    auto val2 = recover_secret(encode_to_shares({(vmsA1[0] % PP), (vmsA2[0] % PP), (vmsA3[0] % PP)}), PP);
//    std::cout << "Trying to reconstruct the first multiplicative value2: " << val2 << std::endl;
//    std::cout << "balance, adjusting for amount: : " << static_cast<float>(val) / amount << std::endl;
    // end remove temp

    // Call the transfer function
    Server server(serverIndex, N);
    server.transfer(kmsA_i, kmsA_i, kmsB_i, tag_A_share, tag_A_share); // TODO: A1 share and tag

    std::cout << "Done" << std::endl;
}

void test_client_deferred(int serverIndex, int logN) {
    int N = 1 << logN;
    int amount = 7;
    int senderIndex = 0;
    int recvIndex = 20;

    // Load/generate data
    std::pair<DPF::DeferredKeyShare, DPF::DeferredKeyShare> kmsAdefer_i;

    auto kmsAdefer = DPF::DeferredGenShamir(senderIndex, logN);
    auto fn = DATA_DIR + "kmsAdefer" + std::to_string(serverIndex) + ".txt";
    if (fileExists(fn)) {
        kmsAdefer_i = loadPair(fn);
    } else {
        // File does not exist
        for (size_t i = 0; i < kmsAdefer.size(); ++i) {
            auto curr_fn = DATA_DIR + "kmsAdefer" + std::to_string(i) + ".txt";
            writePair(kmsAdefer[i], curr_fn);
        }

        kmsAdefer_i = kmsAdefer[serverIndex];
    }

//    field beta = 55; Below are shares of shares of beta
    std::vector<field> beta0 = {847777152, 1485437038, 2123096924};
    std::vector<field> beta1 = {1261625909, 1239392756, 1217159603};
    std::vector<field> beta2 = {1923965764, 58674887, 340867657};

    Server server(serverIndex, N);
    server.evalDeferredTest(kmsAdefer_i, beta0[serverIndex], beta1[serverIndex], beta2[serverIndex]);

    std::cout << "Done" << std::endl;
}

void test_client_malicious(int serverIndex, int logN) {
    int N = 1 << logN;
    int amount = 55;
    int senderIndex = 0;
    int recvIndex = 20;
    uint64_t alpha = 2112445456;

    // Load/generate data
    std::pair<DPF::DeferredKeyShare, DPF::DeferredKeyShare> kmsAdefer_i, kmsA1defer_i;

    // Load/generate data
    std::vector<DPF::KeyShare> kmsA_i;
    std::vector<DPF::KeyShare> kmsA1_i;
    std::vector<DPF::KeyShare> kmsB_i;

    auto kmsA = DPF::GenShamir(senderIndex, logN, amount, false);
    auto kmsA1 = DPF::GenShamir(senderIndex, logN, 1, false);
    auto kmsB = DPF::GenShamir(recvIndex, logN, amount, true);
    auto kmsAdefer = DPF::DeferredGenShamir(senderIndex, logN);
    auto kmsA1defer = DPF::DeferredGenShamir(senderIndex, logN);
    auto fn = DATA_DIR + "kmsAdefer" + std::to_string(serverIndex) + ".txt";
    auto fn1 = DATA_DIR + "kmsA1defer" + std::to_string(serverIndex) + ".txt";

    if (fileExists(fn)) {
        kmsAdefer_i = loadPair(fn);
        kmsA1defer_i = loadPair(fn1);
        kmsA_i = loadVector(serverIndex, "kmsA");
        kmsA1_i = loadVector(serverIndex, "kmsAone");
        kmsB_i = loadVector(serverIndex, "kmsB");
    } else {
        // File does not exist
        for (size_t i = 0; i < kmsAdefer.size(); ++i) {
            auto curr_fn = DATA_DIR + "kmsAdefer" + std::to_string(i) + ".txt";
            auto curr_fn1 = DATA_DIR + "kmsA1defer" + std::to_string(i) + ".txt";
            writePair(kmsAdefer[i], curr_fn);
            writePair(kmsA1defer[i], curr_fn1);
            writeVector(i, kmsA[i], "kmsA");
            writeVector(i, kmsA1[i], "kmsAone");
            writeVector(i, kmsB[i], "kmsB");
        }

        kmsAdefer_i = kmsAdefer[serverIndex];
        kmsA1defer_i = kmsA1defer[serverIndex];
        kmsA_i = kmsA[serverIndex];
        kmsA1_i = kmsA1[serverIndex];
        kmsB_i = kmsB[serverIndex];
    }

    // TODO: This shouldn't work without making sure its deterministic, no? Look into this..
//    field tag_A = (alpha*amount) % PP;
//    std::vector<std::pair<int64_t, int64_t>> tag_A_shares = gen_shares(3, 2, tag_A, PP);
    std::vector<std::pair<int64_t, int64_t>> tag_A_shares = {
            {1, 313768438},
            {2, 407153734},
            {3, 500539030}
    };
    field tag_A_share = tag_A_shares[serverIndex].second;
//    field tag_A1 = (alpha) % PP;
//    std::vector<std::pair<int64_t, int64_t>> tag_A1_shares = gen_shares(3, 2, tag_A1, PP);
    std::vector<std::pair<int64_t, int64_t>> tag_A1_shares = {
            {1, 1710921975},
            {2, 1309398494},
            {3, 907875013}
    };
    field tag_A1_share = tag_A1_shares[serverIndex].second;

//    field beta = 55; Below are shares of shares of amount and ones
    std::vector<field> beta0 = {847777152, 1485437038, 2123096924};
    std::vector<field> beta1 = {1261625909, 1239392756, 1217159603};
    std::vector<field> beta2 = {1923965764, 58674887, 340867657};

//    // Shares of 1 - [[1843535466], [1539587284], [1235639102]]. Below are shares of these shares:
//    std::vector<field> one0 = {1368805972, 894076478, 419346984};
//    std::vector<field> one1 = {1920794995, 154519059, 535726770};
//    std::vector<field> one2 = {158492188, 1228828921, 151682007};

//    Shares of 1 - [[631234535], [1262469069], [1893703603]]
// These shares are of [[631234535], [1262469069*modinv(2)], [1893703603*modinv(3)]]
//    [[951264990], [1271295445], [1591325900]]
//    [[957422624], [209868890], [1609798803]]
//    [[1729613830], [1396337361], [1063060892]]
    std::vector<field> one0 = {951264990, 1271295445, 1591325900};
    std::vector<field> one1 = {957422624, 209868890, 1609798803};
    std::vector<field> one2 = {1729613830, 1396337361, 1063060892};

    Server server(serverIndex, N);
    server.transferMalicious(kmsA_i, kmsAdefer_i, kmsA1_i, kmsA1defer_i, kmsB_i, tag_A_share, tag_A1_share, beta0[serverIndex], beta1[serverIndex], beta2[serverIndex], one0[serverIndex], one1[serverIndex], one2[serverIndex]);

    std::cout << "Done" << std::endl;
}

// Helper function - mostly temp, to help debug what happens in the deferred stuf
void deconstruct_deferreddpf() {
    std::vector<std::pair<DPF::DeferredKeyShare, DPF::DeferredKeyShare>> kmsA1defer;
    std::vector<std::vector<std::pair<uint8_t , uint8_t>>> key0_s0_shares, key1_s0_shares, key0_s1_shares, key1_s1_shares;
    std::vector<std::pair<uint8_t, uint8_t>> key0_t0_shares, key1_t0_shares;

    for (int i = 0; i < 3; i++) {
//        auto fn1 = DATA_DIR + "kmsA1defer" + std::to_string(i) + ".txt";
        auto fn1 = DATA_DIR + "kmsAdefer" + std::to_string(i) + ".txt";
        auto kmsA1defer_i = loadPair(fn1);
        kmsA1defer.push_back(kmsA1defer_i);
        key0_s0_shares.push_back(kmsA1defer_i.first.s0_share);
        key0_s1_shares.push_back(kmsA1defer_i.first.s1_share);
        key1_s0_shares.push_back(kmsA1defer_i.second.s0_share);
        key1_s1_shares.push_back(kmsA1defer_i.second.s1_share);
        key0_t0_shares.push_back(kmsA1defer_i.first.t0_share);
        key1_t0_shares.push_back(kmsA1defer_i.second.t0_share);
    }

    std::vector<uint8_t> key0_s0, key0_s1, key1_s0, key1_s1;
    for (int i = 0; i < key0_s0_shares[0].size(); i++) {
        key0_s0.push_back(reconstruct_gf256({key0_s0_shares[0][i], key0_s0_shares[1][i], key0_s0_shares[2][i]}));
        key0_s1.push_back(reconstruct_gf256({key0_s1_shares[0][i], key0_s1_shares[1][i], key0_s1_shares[2][i]}));
        key1_s0.push_back(reconstruct_gf256({key1_s0_shares[0][i], key1_s0_shares[1][i], key1_s0_shares[2][i]}));
        key1_s1.push_back(reconstruct_gf256({key1_s1_shares[0][i], key1_s1_shares[1][i], key1_s1_shares[2][i]}));
    }

    std::cout << "key0, s0: ";
    printVector(key0_s0);
    std::cout << "key0, s1: ";
    printVector(key0_s1);
    std::cout << "key1, s0: ";
    printVector(key1_s0);
    std::cout << "key1, s1: ";
    printVector(key1_s1);

    auto key0_t0 = reconstruct_gf256(key0_t0_shares);
    auto key1_t0 = reconstruct_gf256(key1_t0_shares);
    std::cout << "key0, t0: " << int(key0_t0) << std::endl;
    std::cout << "key1, t0: " << int(key1_t0) << std::endl;
}

void test_server(int serverIndex, int logN) {
    int N = 1 << logN;

    Server server(serverIndex, N);

    auto kms1 = DPF::GenShamir(15, logN, 150);
    auto kms2 = DPF::GenShamir(20, logN, 300);
    uint32_t tag_share = 54;

    // Call the transfer function
//    server.transfer(kms1[0], kms2[0], tag_share); //TODO: index to kms1, kms2 based on server_index

    std::cout << "Done" << std::endl;
}

// test gf256 shares library
void test_shamir_gf256() {

    // Example usage
    int secret = 123; // The secret to share
    int n = 3;       // Total number of shares
    int k = 1;       // Threshold

    auto shares = share_gf256(secret, n, k);
    // Display shares
    for (const auto& share : shares) {
        std::cout << "Share " << static_cast<int>(share.first) << ": " << static_cast<int>(share.second) << std::endl;
    }

    // Reconstruction using any k shares
//    int reconstructed_secret = reconstruct_gf256({shares[0], shares[1], shares[2]});
    int reconstructed_secret = reconstruct_gf256({shares[0], shares[1]});
    std::cout << "Reconstructed Secret: " << reconstructed_secret << std::endl;


    // Vector of shares

    // Example usage with a vector of integers
    std::vector<uint8_t> secrets = {123, 45, 67, 89}; // The secrets to share

    auto all_shares = share_gf256_vector(secrets, n, k);

    // Display shares for each secret
    for (size_t i = 0; i < all_shares.size(); ++i) {
        std::cout << "Secret " << i + 1 << " Shares:" << std::endl;
        for (const auto& share : all_shares[i]) {
            std::cout << "  Share " << share.first << ": " << share.second << std::endl;
        }
    }

    // Reconstruction using any k shares from each set
    std::vector<std::vector<std::pair<uint8_t, uint8_t>>> selected_shares;
    for (const auto& shares : all_shares) {
        selected_shares.push_back({shares[0], shares[1]});
    }
    auto reconstructed_secrets = reconstruct_gf256_vector(selected_shares);

    // Display reconstructed secrets
    std::cout << "Reconstructed Secrets:" << std::endl;
    for (int secret : reconstructed_secrets) {
        std::cout << secret << " ";
    }
    std::cout << std::endl;


    // Test XOR
    // Example usage with two vectors of integers
    std::vector<uint8_t> x = {123, 45, 67, 89}; // First vector of secrets
    std::vector<uint8_t> y = {12, 34, 56, 78};  // Second vector of secrets

    auto x_shares = share_gf256_vector(x, n, k);
    auto y_shares = share_gf256_vector(y, n, k);

    // Generate shares of z = x ^ y using the refactored xor_shares_vector function
    std::vector<std::vector<std::pair<uint8_t, uint8_t>>> z_shares(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        z_shares[i] = xor_shares_vector(x_shares[i], y_shares[i]);
    }

    // Reconstruct z and verify it equals x ^ y
    auto reconstructed_z = reconstruct_gf256_vector(z_shares);
    std::vector<int> expected_z;
    for (size_t i = 0; i < x.size(); ++i) {
        expected_z.push_back((x[i] ^ y[i]));
    }

    // Display results
    std::cout << "Reconstructed Z: ";
    for (int val : reconstructed_z) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    std::cout << "Expected Z: ";
    for (int val : expected_z) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

}

int reconstruct_fake_xor_rand() {
    auto a = fake_xor_rand(0);
    auto b = fake_xor_rand(1);
    auto c = fake_xor_rand(2);

    for (int i = 0; i < a.size(); i++) {
        auto v = reconstruct_gf256({std::make_pair(1, a[i].second), std::make_pair(2, b[i].second), std::make_pair(3, c[i].second)});
        std::cout << int(v) << std::endl;
    }
//    auto res = reconstruct_gf256_vector({a, b, c});

}

void test_shares_mult_gf256() {
    uint8_t pub = 176;
    auto t0_shares = share_gf256(0, 3, 1);
    uint8_t t1_val = 0xFF;
    auto t1_shares = share_gf256(t1_val, 3, 1);

    std::vector<std::pair<uint8_t, uint8_t>> res0, res1;

    for (int i = 0; i < t0_shares.size(); i++) {
        res0.push_back(std::make_pair(t0_shares[i].first, t0_shares[i].second & pub));
        res1.push_back(std::make_pair(t1_shares[i].first, t1_shares[i].second & pub));
    }
    auto v0 = reconstruct_gf256(res0);
    auto v1 = reconstruct_gf256(res1);

    assert(v0 == (0 & pub));
    assert(v1 == (t1_val & pub));
    std::cout << "Test done!" << std::endl;
}

void mock_fproduct_test(int logN) {
    int N = 1 << logN;
    int amount = 55;
    int senderIndex = 0;
    int recvIndex = 20;
    uint64_t alpha = 2112445456;

    // Load/generate data
    std::vector<std::vector<DPF::KeyShare>> kmsA;
    std::vector<std::vector<DPF::KeyShare>> kmsA1;
    std::vector<std::vector<DPF::KeyShare>> kmsB;

    Server server0(0, N, true);
    Server server1(1, N, true);
    Server server2(2, N, true);

    std::vector<Server> servers = {server0, server1, server2};
    std::vector<std::pair<int64_t, int64_t>> tag_shares, balance_shares, tag_delta_shares;

    std::vector<std::pair<int64_t, int64_t>> tag_A_shares = {
            {1, 313768438},
            {2, 407153734},
            {3, 500539030}
    };

    for (size_t i = 0; i < 3; ++i) {
        kmsA.push_back(loadVector(i, "kmsA"));
        kmsA1.push_back(loadVector(i, "kmsAone"));
        kmsB.push_back(loadVector(i, "kmsB"));

        auto res_A = DPF::EvalShamir(kmsA[i], logN, i, false);
        auto res_A1 = DPF::EvalShamir(kmsA1[i], logN, i, false);
        auto res_B = DPF::EvalShamir(kmsB[i], logN, i, true);

        auto data_A = res_A.first;
        auto data_A1 = res_A1.first;
        auto data_B = res_B.first;

        field tag = mod(static_cast<int64_t>(PIRW::innerprodff31(servers[i].alphas, data_A)), PP);
//        field tag_share_A1_prime = mod(static_cast<int64_t>(PIRW::innerprodff31(alphas, data_A1)), PP);
        field balance = mod(static_cast<int64_t>(PIRW::innerprodff31(data_A, servers[i].ledger)), PP);
        field tag_delta_A_share = mod(static_cast<int64_t>(tag_A_shares[i].second) - tag, PP);

        tag_shares.push_back(std::make_pair(i + 1, tag));
        std::cout << "i: " << i << ", and tag: " << tag << std::endl;
        balance_shares.push_back(std::make_pair(i + 1, balance));
        tag_delta_shares.push_back(std::make_pair(i + 1, tag_delta_A_share));
    }

    auto tag_reconstructed = recover_secret(tag_shares, PP);
    auto balance_reconstructed = recover_secret(balance_shares, PP);
    auto tag_delta_reconstructed = recover_secret(tag_delta_shares, PP);

    std::cout << tag_reconstructed << ", and: " << balance_reconstructed << ", and tag_delta:" << tag_delta_reconstructed << std::endl;

}

int main(int argc, char** argv) {
    std::cout << "Current working directory: "
              << std::filesystem::current_path()
              << std::endl;

    if(argc != 3) {
	    std::cout << "Usage: ./dpf_pir <server_index> <log_tree_size>" << std::endl;
        return -1;
    }

    generate_tables();
//    print_fake_block_sharing();

//    testSerialization();
//    print_fake_zero_triplets_code();
//    test_shamir_gf256();
//    test_hash_functions();
    size_t N = std::strtoull(argv[2], nullptr, 10);
//    int x = run_playground_tests(N); // misc tests
//    reconstruct_fake_xor_rand();
//    deconstruct_deferreddpf();
//    test_shares_mult_gf256();
    mock_fproduct_test(N);
    int serverIndex = std::atoi(argv[1]);


    // Benchmark mersenne modulus
//    benchmark_mersenne();
//    test_sumproduct();

//    test_server(serverIndex, N);
//    test_client_deferred(serverIndex, N);
//    test_client(serverIndex, N);
    test_client_malicious(serverIndex, N);

    return 0;
}
