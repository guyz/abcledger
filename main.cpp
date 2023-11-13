#include "dpf.h"
#include "pirw.h"

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

int main(int argc, char** argv) {

    if(argc != 2) {
	    std::cout << "Usage: ./dpf_pir <log_tree_size>" << std::endl;
        return -1;
    }
    size_t N = std::strtoull(argv[1], nullptr, 10);
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

    // Benchmark mersenne modulus
    benchmark_mersenne();

    return 0;
}
