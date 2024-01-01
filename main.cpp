#include "dpf.h"
#include "pirw.h"
#include "Server.h"
#include <chrono>
#include <iostream>
#include <cassert>
#include <immintrin.h>
#include "sss/sss.h"
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


inline __m256i mul256(__m256i x1, __m256i x2) {
    return _mm256_mullo_epi64(x1, x2);
}

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
        auto vmp0 = DPF::EvalFull8P(kmp0, N);
        auto vmp1 = DPF::EvalFull8P(kmp1, N, true);

        auto kms = DPF::GenShamir(alpha, N, beta);
        auto kms0 = kms[0];
        auto kms1 = kms[1];
        auto kms2 = kms[2];
        auto vms0 = DPF::EvalShamir(kms0, N, 0);
        auto vms1 = DPF::EvalShamir(kms1, N, 1);
        auto vms2 = DPF::EvalShamir(kms2, N, 2);

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

// TODO: refactor...
// Serialize
void serialize(const std::vector<std::pair<uint32_t, std::vector<uint8_t>>>& data, const std::string& filename) {
    std::ofstream out(filename, std::ios::binary);
    for (const auto& pair : data) {
        out.write(reinterpret_cast<const char*>(&pair.first), sizeof(pair.first));
        uint32_t size = pair.second.size();
        out.write(reinterpret_cast<const char*>(&size), sizeof(size));
        out.write(reinterpret_cast<const char*>(pair.second.data()), size);
    }
    out.close();
}

// Unserialize
std::vector<std::pair<uint32_t, std::vector<uint8_t>>> unserialize(const std::string& filename) {
    std::vector<std::pair<uint32_t, std::vector<uint8_t>>> data;
    std::ifstream in(filename, std::ios::binary);
    while (!in.eof()) {
        uint32_t first;
        uint32_t size;
        in.read(reinterpret_cast<char*>(&first), sizeof(first));
        in.read(reinterpret_cast<char*>(&size), sizeof(size));
        std::vector<uint8_t> second(size);
        in.read(reinterpret_cast<char*>(second.data()), size);
        data.emplace_back(first, second);
    }
    in.close();
    return data;
}


void writeVector(int index, const std::vector<std::pair<uint32_t, std::vector<uint8_t>>>& kms, std::string filename) {
    serialize(kms, "data/" + filename + std::to_string(index) + ".txt");
}

std::vector<std::pair<uint32_t, std::vector<uint8_t>>> loadVector(int index, std::string filename) {
    return unserialize("data/" + filename + std::to_string(index) + ".txt");
}

bool fileExists(const std::string& fileName) {
    return std::filesystem::exists(fileName);
}

void test_client(int serverIndex, int logN) {
    int N = 1 << logN;
    int amount = 7;
    int senderIndex = 0;
    int recvIndex = 20;
    uint64_t alpha = 1365547451;

    // Load/generate data
    std::vector<std::pair<uint32_t, std::vector<uint8_t>>> kmsA_i;
    std::vector<std::pair<uint32_t, std::vector<uint8_t>>> kmsB_i;

    auto kmsA = DPF::GenShamir(senderIndex, logN, amount);
    auto kmsB = DPF::GenShamir(recvIndex, logN, amount);

    if (fileExists("data/kmsA" + std::to_string(serverIndex) + ".txt")) {
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

    uint32_t tag_share = (alpha*amount) % PP; // TODO: shamir share this..

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
    server.transfer(kmsA_i, kmsB_i, tag_share);

    std::cout << "Done" << std::endl;
}

void test_server(int serverIndex, int logN) {
    int N = 1 << logN;

    Server server(serverIndex, N);

    auto kms1 = DPF::GenShamir(15, logN, 150);
    auto kms2 = DPF::GenShamir(20, logN, 300);
    uint32_t tag_share = 54;

    // Call the transfer function
    server.transfer(kms1[0], kms2[0], tag_share); //TODO: index to kms1, kms2 based on server_index

    std::cout << "Done" << std::endl;
}

int main(int argc, char** argv) {

    if(argc != 3) {
	    std::cout << "Usage: ./dpf_pir <server_index> <log_tree_size>" << std::endl;
        return -1;
    }

//    test_hash_functions();
    size_t N = std::strtoull(argv[2], nullptr, 10);
    int x = run_playground_tests(N); // misc tests

    int serverIndex = std::atoi(argv[1]);


    // Benchmark mersenne modulus
//    benchmark_mersenne();
//    test_sumproduct();

//    test_server(serverIndex, N);
    test_client(serverIndex, N);

    return 0;
}
